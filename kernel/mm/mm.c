// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/interrupt/ipi.h"
#include "mos/mm/cow.h"
#include "mos/mm/mmstat.h"
#include "mos/mm/paging/dump.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <stdlib.h>
#include <string.h>

static slab_t *vmap_cache = NULL;
SLAB_AUTOINIT("vmap", vmap_cache, vmap_t);

static slab_t *mm_context_cache = NULL;
SLAB_AUTOINIT("mm_context", mm_context_cache, mm_context_t);

phyframe_t *mm_get_free_page_raw(void)
{
    phyframe_t *frame = pmm_allocate_frames(1, PMM_ALLOC_NORMAL);
    if (!frame)
    {
        pr_emerg("failed to allocate a page");
        return NULL;
    }

    return frame;
}

phyframe_t *mm_get_free_page(void)
{
    phyframe_t *frame = mm_get_free_page_raw();
    if (!frame)
        return NULL;
    memzero((void *) phyframe_va(frame), MOS_PAGE_SIZE);
    return frame;
}

phyframe_t *mm_get_free_pages(size_t npages)
{
    phyframe_t *frame = pmm_allocate_frames(npages, PMM_ALLOC_NORMAL);
    if (!frame)
    {
        pr_emerg("failed to allocate %zd pages", npages);
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
    mm_context_t *mmctx = kmalloc(mm_context_cache);
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
    if (ctx1 == ctx2 || ctx2 == NULL)
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
    if (ctx1 == ctx2 || ctx2 == NULL)
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

mm_context_t *mm_switch_context(mm_context_t *new_ctx)
{
    mm_context_t *old_ctx = current_cpu->mm_context;
    if (old_ctx == new_ctx)
        return old_ctx;

    platform_switch_mm(new_ctx);
    current_cpu->mm_context = new_ctx;
    return old_ctx;
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
    vmap_t *map = kmalloc(vmap_cache);
    linked_list_init(list_node(map));
    spinlock_acquire(&map->lock);
    map->vaddr = vaddr;
    map->npages = npages;
    do_attach_vmap(mmctx, map);
    return map;
}

void vmap_destroy(vmap_t *vmap)
{
    MOS_ASSERT(spinlock_is_locked(&vmap->lock));
    mm_context_t *const mm = vmap->mmctx;

    spinlock_acquire(&mm->mm_lock);
    mm_do_unmap(mm->pgd, vmap->vaddr, vmap->npages, true);
    list_remove(vmap);
    spinlock_release(&mm->mm_lock);
    kfree(vmap);
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

void vmap_finalise_init(vmap_t *vmap, vmap_content_t content, vmap_type_t type)
{
    MOS_ASSERT(spinlock_is_locked(&vmap->lock));
    MOS_ASSERT_X(content != VMAP_UNKNOWN, "vmap content cannot be unknown");
    MOS_ASSERT_X(vmap->content == VMAP_UNKNOWN || vmap->content == content, "vmap is already setup");

    vmap->content = content;
    vmap->type = type;
    spinlock_release(&vmap->lock);
}

static bool fallback_zero_on_demand_fault_handler(vmap_t *vmap, ptr_t fault_addr, const pagefault_t *info)
{
    // this will be used if no pagefault handler has been registered, ie. for anonymous mappings
    // which does not have a backing file, nor CoW pages

    if (info->is_present)
    {
        // there is a mapping, but ATM we don't know how to handle that fault
        // CoW fault should use cow_fault_handler()
        return false;
    }

    phyframe_t *frame = mm_get_free_page();
    if (unlikely(!frame))
    {
        pr_emerg("Out of memory");
        return false;
    }
    mm_do_map(vmap->mmctx->pgd, fault_addr, phyframe_pfn(frame), 1, info->is_user ? VM_USER_RW : VM_RW, true);
    vmap_mstat_inc(vmap, inmem, 1);
    return true;
}

bool mm_handle_fault(ptr_t fault_addr, pagefault_t *info)
{
    mm_context_t *const mm = current_mm;
    mm_lock_ctx_pair(mm, NULL);

    size_t offset;
    vmap_t *fault_vmap = vmap_obtain(mm, fault_addr, &offset);
    if (!fault_vmap)
    {
        pr_emph("page fault in " PTR_FMT " (not mapped)", fault_addr);
        mm_unlock_ctx_pair(mm, NULL);
        return false;
    }

    if (info->is_write && !(fault_vmap->vmflags & VM_WRITE))
    {
        pr_emph("page fault in " PTR_FMT " (read-only)", fault_addr);
        mm_unlock_ctx_pair(mm, NULL);
        return false;
    }

    mos_debug(cow, "thread=%pt, fault_addr=" PTR_FMT ", handler=%ps", (void *) current_thread, fault_addr, (void *) fault_vmap->on_fault);

    if (info->is_present)
        info->faulting_frame = pfn_phyframe(mm_do_get_pfn(fault_vmap->mmctx->pgd, fault_addr));

    bool result = false;
    if (fault_vmap->on_fault)
    {
        static const char *const fault_result_names[] = {
            [VMFAULT_OK] = "OK",
            [VMFAULT_GOT_BACKING_PAGE] = "GOT_BACKING_PAGE",
            [VMFAULT_CANNOT_HANDLE] = "CANNOT_HANDLE",
        };

        vmfault_result_t fault_result = fault_vmap->on_fault(fault_vmap, fault_addr, info);
        mos_debug(cow, "fault handler returned %s (%d)", fault_result_names[fault_result], fault_result);
        switch (fault_result)
        {
            case VMFAULT_OK: result = true; goto done;
            case VMFAULT_CANNOT_HANDLE: goto cannot_handle;
            case VMFAULT_GOT_BACKING_PAGE:
            {
                MOS_ASSERT(info->backing_page);

                vm_flags flags = fault_vmap->vmflags;
                phyframe_t *page = info->backing_page;

                if (fault_vmap->type == VMAP_TYPE_PRIVATE && info->is_write)
                {
                    // We perform copying iff ((the page is private) AND (the user is writing to it))
                    // If it's a private page, but the user is only reading from it, we can just map it as read-only
                    //   so that a CoW fault can be triggered later when the user writes to it
                    //   this saves us from copying the page unnecessarily
                    page = mm_get_free_page();
                    vmap_mstat_inc(fault_vmap, inmem, 1);
                    memcpy((void *) phyframe_va(page), (void *) phyframe_va(info->backing_page), MOS_PAGE_SIZE);
                }
                else
                {
                    flags &= ~VM_WRITE;
                }

                mm_replace_page_locked(fault_vmap->mmctx, fault_addr, phyframe_pfn(page), flags);
                ipi_send_all(IPI_TYPE_INVALIDATE_TLB);
                result = true;
            }
        }
    }
    else
    {
    cannot_handle:
        result = fallback_zero_on_demand_fault_handler(fault_vmap, fault_addr, info);
    }

done:
    spinlock_release(&fault_vmap->lock);
    mm_unlock_ctx_pair(mm, NULL);
    return result;
}

// ! sysfs support

static bool sys_mem_mmap(sysfs_file_t *f, vmap_t *vmap, off_t offset)
{
    MOS_UNUSED(f);
    mm_do_map(vmap->mmctx->pgd, vmap->vaddr, offset / MOS_PAGE_SIZE, vmap->npages, vmap->vmflags, false);
    return true;
}

static sysfs_item_t sys_mem_item = SYSFS_MEM_ITEM("mem", sys_mem_mmap);

static void mm_sysfs_init()
{
    sys_mem_item.mem.size = platform_info->max_pfn * MOS_PAGE_SIZE;
    sysfs_register_root_file(&sys_mem_item, NULL);
}

MOS_INIT(SYSFS, mm_sysfs_init);
