// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"

#include "mos/mm/cow.h"
#include "mos/mm/paging/dump.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <string.h>

static slab_t *vmap_cache = NULL;
MOS_SLAB_AUTOINIT("vmap", vmap_cache, vmap_t);

static slab_t *mm_context_cache = NULL;
MOS_SLAB_AUTOINIT("mm_context", mm_context_cache, mm_context_t);

void mos_kernel_mm_init(void)
{
    pr_info("initializing kernel memory management");

    cow_init();
    slab_init();

#if MOS_DEBUG_FEATURE(vmm)
    declare_panic_hook(mm_dump_current_pagetable, "Dump page table");
    install_panic_hook(&mm_dump_current_pagetable_holder);
    mm_dump_current_pagetable();
#endif
}

phyframe_t *mm_get_free_page_raw(void)
{
    phyframe_t *frame = pmm_allocate_frames(1, PMM_ALLOC_NORMAL);
    if (!frame)
    {
        mos_warn("Failed to allocate a page");
        return NULL;
    }

    return frame;
}

phyframe_t *mm_get_free_page(void)
{
    phyframe_t *frame = mm_get_free_page_raw();
    memzero((void *) phyframe_va(frame), MOS_PAGE_SIZE);
    return frame;
}

phyframe_t *mm_get_free_pages(size_t npages)
{
    phyframe_t *frame = pmm_allocate_frames(npages, PMM_ALLOC_NORMAL);
    if (!frame)
    {
        mos_warn("Failed to allocate %zd pages", npages);
        return NULL;
    }

    return frame;
}

void mm_free_page(phyframe_t *frame)
{
    pmm_free_frames(frame, 1);
}

void mm_free_pages(phyframe_t *frame, size_t npages)
{
    pmm_free_frames(frame, npages);
}

mm_context_t *mm_create_context(void)
{
    mm_context_t *mmctx = kmemcache_alloc(mm_context_cache);
    linked_list_init(&mmctx->mmaps);

    pml4_t pml4 = pml_create_table(pml4);

    // map the upper half of the address space to the kernel
    for (int i = pml4_index(MOS_KERNEL_START_VADDR); i < PML4_ENTRIES; i++)
        pml4.table[i] = platform_info->kernel_mm->pgd.max.next.table[i];

    mmctx->pgd = pgd_create(pml4);

    return mmctx;
}

void mm_destroy_context(mm_context_t *mmctx)
{
    MOS_UNUSED(mmctx);
    MOS_UNIMPLEMENTED("mm_destroy_context");
}

void mm_lock_ctx_pair(mm_context_t *ctx1, mm_context_t *ctx2)
{
    if (ctx1 == ctx2)
        spinlock_acquire(&ctx1->mm_lock);
    else if (ctx1 < ctx2)
    {
        spinlock_acquire(&ctx1->mm_lock);
        spinlock_acquire(&ctx2->mm_lock);
    }
    else
    {
        spinlock_acquire(&ctx2->mm_lock);
        spinlock_acquire(&ctx1->mm_lock);
    }
}

void mm_unlock_ctx_pair(mm_context_t *ctx1, mm_context_t *ctx2)
{
    if (ctx1 == ctx2)
        spinlock_release(&ctx1->mm_lock);
    else if (ctx1 < ctx2)
    {
        spinlock_release(&ctx2->mm_lock);
        spinlock_release(&ctx1->mm_lock);
    }
    else
    {
        spinlock_release(&ctx1->mm_lock);
        spinlock_release(&ctx2->mm_lock);
    }
}

static void do_attach_vmap(mm_context_t *mmctx, vmap_t *vmap)
{
    MOS_ASSERT(spinlock_is_locked(&mmctx->mm_lock));
    MOS_ASSERT_X(list_is_empty(list_node(vmap)), "vmap is already attached to something");
    MOS_ASSERT(vmap->mmctx == NULL || vmap->mmctx == mmctx);

    vmap->mmctx = mmctx;

    // add to the list, sorted by address
    list_foreach(vmap_t, m, mmctx->mmaps)
    {
        if (m->vaddr > vmap->vaddr)
        {
            list_insert_before(m, vmap);
            return;
        }
    }

    list_node_append(&mmctx->mmaps, list_node(vmap)); // append at the end
}

vmap_t *vmap_create(mm_context_t *mmctx, ptr_t vaddr, size_t npages)
{
    MOS_ASSERT_X(mmctx != platform_info->kernel_mm, "you can't create vmaps in the kernel mmctx");
    vmap_t *map = kmemcache_alloc(vmap_cache);
    linked_list_init(list_node(map));
    spinlock_acquire(&map->lock);
    map->vaddr = vaddr;
    map->npages = npages;
    do_attach_vmap(mmctx, map);
    return map;
}

vmap_t *vmap_obtain(mm_context_t *mmctx, ptr_t vaddr, size_t *out_offset)
{
    MOS_ASSERT(spinlock_is_locked(&mmctx->mm_lock));

    list_foreach(vmap_t, m, mmctx->mmaps)
    {
        if (m->vaddr <= vaddr && vaddr < m->vaddr + m->npages * MOS_PAGE_SIZE)
        {
            spinlock_acquire(&m->lock);
            if (out_offset)
                *out_offset = vaddr - m->vaddr;
            return m;
        }
    }

    if (out_offset)
        *out_offset = 0;
    return NULL;
}

void vmap_finalise_init(vmap_t *vmap, vmap_content_t content, vmap_fork_behavior_t fork_behavior)
{
    MOS_ASSERT(spinlock_is_locked(&vmap->lock));
    MOS_ASSERT_X(content != VMAP_UNKNOWN, "vmap content cannot be unknown");
    MOS_ASSERT_X(vmap->content == VMAP_UNKNOWN || vmap->content == content, "vmap is already setup");

    vmap->content = content;
    vmap->fork_behavior = fork_behavior;
    spinlock_release(&vmap->lock);
}

static bool fallback_fault_handler(vmap_t *vmap, ptr_t fault_addr, const pagefault_info_t *info)
{
    // this will be used if no pagefault handler has been registered, ie. for anonymous mappings
    // which does not have a backing file, nor CoW pages

    if (info->page_present)
    {
        // there is a mapping, but ATM we don't know how to handle that fault
        // CoW fault should use cow_fault_handler()
        return false;
    }

    phyframe_t *frame = mm_get_free_page();
    mm_do_map(vmap->mmctx->pgd, fault_addr, phyframe_pfn(frame), 1, info->userfault ? VM_USER_RW : VM_RW, true);
    vmap_mstat_inc(vmap, inmem, 1);
    return true;
}

bool mm_handle_fault(ptr_t fault_addr, const pagefault_info_t *info)
{
    if (unlikely(!current_thread))
    {
        pr_warn("There shouldn't be a page fault before the scheduler is initialized!");
        return false;
    }

    mm_context_t *const mm = current_mm;
    MOS_ASSERT_X(mm == current_cpu->mm_context, "Page fault in a process that is not the current process?!");
#if MOS_DEBUG_FEATURE(cow)
    process_dump_mmaps(current_process);
#endif

    spinlock_acquire(&mm->mm_lock);

    vmap_t *fault_vmap = vmap_obtain(mm, fault_addr, NULL); // vmap is locked
    if (!fault_vmap)
    {
        pr_emph("page fault in " PTR_FMT " (not mapped)", fault_addr);
        spinlock_release(&mm->mm_lock);
        return false;
    }

    mos_debug(vmm, "pid=%d, tid=%d, fault_addr=" PTR_FMT ", vmblock=" PTR_RANGE ", handler=%s", current_thread->tid, current_process->pid, fault_addr, fault_vmap->vaddr,
              fault_vmap->vaddr + fault_vmap->npages * MOS_PAGE_SIZE - 1, fault_vmap->on_fault ? "registered" : "fallback");

    bool result = fault_vmap->on_fault && fault_vmap->on_fault(fault_vmap, fault_addr, info);

    if (!result)
        result = fallback_fault_handler(fault_vmap, fault_addr, info);

    spinlock_release(&fault_vmap->lock);
    spinlock_release(&mm->mm_lock);
    return result;
}
