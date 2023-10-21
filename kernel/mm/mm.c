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
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/signal.h"
#include "mos/tasks/task_types.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos_stdlib.h>
#include <mos_string.h>

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
    mm_free_pages(frame, 1);
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
    MOS_ASSERT(spinlock_is_locked(&mm->mm_lock));
    if (vmap->io)
    {
        io_unref(vmap->io);
        // if (!io_munmap(range_map->io, range_map))
        // {
        //     pr_warn("munmap: could not unmap the file: io_munmap() failed");
        //     return false;
        // }
    }
    mm_do_unmap(mm->pgd, vmap->vaddr, vmap->npages, true);
    list_remove(vmap);
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

vmap_t *vmap_split(vmap_t *first, size_t split)
{
    MOS_ASSERT(spinlock_is_locked(&first->lock));
    MOS_ASSERT(split && split < first->npages);

    vmap_t *second = kmalloc(vmap_cache);
    *second = *first;                    // copy the whole structure
    linked_list_init(list_node(second)); // except for the list node

    first->npages = split; // shrink the first vmap
    second->npages -= split;
    second->vaddr += split * MOS_PAGE_SIZE;
    if (first->io)
    {
        second->io = io_ref(first->io); // ref the io again
        second->io_offset += split * MOS_PAGE_SIZE;
    }

    do_attach_vmap(first->mmctx, second);
    return second;
}

vmap_t *vmap_split_for_range(vmap_t *vmap, size_t rstart_pgoff, size_t rend_pgoff)
{
    MOS_ASSERT(spinlock_is_locked(&vmap->lock));

    /// |-------|-------|-------|
    /// |begin  |rstart |rend   |end
    /// |-------|-------|-------|

    if (rstart_pgoff == 0 && rend_pgoff == vmap->npages)
        return vmap;

    if (rstart_pgoff == 0)
        return vmap_split(vmap, rend_pgoff);

    if (rend_pgoff == vmap->npages)
        return vmap_split(vmap, rstart_pgoff);

    vmap_t *second = vmap_split(vmap, rstart_pgoff);
    vmap_t *third = vmap_split(second, rend_pgoff - rstart_pgoff);
    spinlock_release(&third->lock);
    return second;
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

void mm_copy_page(const phyframe_t *src, const phyframe_t *dst)
{
    memcpy((void *) phyframe_va(dst), (void *) phyframe_va(src), MOS_PAGE_SIZE);
}

vmfault_result_t mm_resolve_cow_fault(vmap_t *vmap, ptr_t fault_addr, pagefault_t *info)
{
    MOS_ASSERT(spinlock_is_locked(&vmap->lock));
    MOS_ASSERT(info->is_write && info->is_present);

    // fast path to handle CoW
    phyframe_t *page = mm_get_free_page();
    mm_copy_page(info->faulting_page, page);
    mm_replace_page_locked(vmap->mmctx, fault_addr, phyframe_pfn(page), vmap->vmflags);

    return VMFAULT_COMPLETE;
}

static void invalid_page_fault(ptr_t fault_addr, vmap_t *faulting_vmap, pagefault_t *info, const char *unhandled_reason)
{
    pr_emerg("unhandled page fault: %s", unhandled_reason);
#if MOS_CONFIG(MOS_MM_DETAILED_UNHANDLED_FAULT)
    pr_emerg("  invalid %s mode %s %s page [" PTR_FMT "]",                               //
             info->is_user ? "user" : "kernel",                                          //
             info->is_write ? "write to" : (info->is_exec ? "execute in" : "read from"), //
             info->is_present ? "present" : "non-present",                               //
             fault_addr                                                                  //
    );
    pr_emerg("  instruction: " PTR_FMT, info->instruction);
    pr_emerg("  thread: %pt", (void *) current_thread);
    pr_emerg("  process: %pp", current_thread ? (void *) current_process : NULL);

    if (fault_addr < 1 KB)
    {
        if (info->is_write)
            pr_emerg("  write to NULL pointer");
        else if (info->is_exec && fault_addr == 0)
            pr_emerg("  attempted to execute NULL pointer");
        else
            pr_emerg("  possible NULL pointer dereference");
    }

    if (info->is_user && fault_addr > MOS_KERNEL_START_VADDR)
        pr_emerg("  kernel address dereference");

    if (info->instruction > MOS_KERNEL_START_VADDR)
        pr_emerg("  in kernel function %ps", (void *) info->instruction);

    if (faulting_vmap)
        pr_emerg("  in vmap: %pvm", (void *) faulting_vmap);

    if (current_thread)
        process_dump_mmaps(current_process);

    pr_info("stack trace before fault (may be unreliable):");
    platform_dump_stack(info->regs);

    pr_info("register states before fault:");
    platform_dump_regs(info->regs);
    pr_cont("\n");
#else
    MOS_UNUSED(faulting_vmap);
    MOS_UNUSED(fault_addr);
    MOS_UNUSED(info);
#endif

    if (current_thread)
    {
        signal_send_to_thread(current_thread, SIGSEGV);
        signal_check_and_handle();
        MOS_UNREACHABLE();
    }

    mos_panic("unhandled page fault");
}

void mm_handle_fault(ptr_t fault_addr, pagefault_t *info)
{
    thread_t *current = current_thread;
    const char *unhandled_reason = NULL;

    if (MOS_DEBUG_FEATURE(cow))
    {
        if (current)
        {
            pr_emph("%s page fault: thread %pt, process %pp at " PTR_FMT ", instruction " PTR_FMT, //
                    info->is_user ? "user" : "kernel",                                             //
                    (void *) current,                                                              //
                    (void *) current->owner,                                                       //
                    fault_addr,                                                                    //
                    info->instruction                                                              //
            );
        }
        else
        {
            pr_emph("%s page fault: kernel thread at " PTR_FMT ", instruction " PTR_FMT, //
                    info->is_user ? "user" : "kernel",                                   //
                    fault_addr,                                                          //
                    info->instruction                                                    //
            );
        }
    }

    if (info->is_write && info->is_exec)
        mos_panic("Cannot write and execute at the same time");

    mm_context_t *const mm = current_mm;
    mm_lock_ctx_pair(mm, NULL);

    size_t offset;
    vmap_t *fault_vmap = vmap_obtain(mm, fault_addr, &offset);
    if (!fault_vmap)
    {
        unhandled_reason = "page fault in unmapped area";
        mm_unlock_ctx_pair(mm, NULL);
        goto unhandled_fault;
    }

    MOS_ASSERT_X(fault_vmap->on_fault, "vmap %pvm has no fault handler", (void *) fault_vmap);
    const vm_flags page_flags = mm_do_get_flags(fault_vmap->mmctx->pgd, fault_addr);

    if (info->is_exec && !(fault_vmap->vmflags & VM_EXEC))
    {
        unhandled_reason = "page fault in non-executable vmap";
        mm_unlock_ctx_pair(mm, NULL);
        goto unhandled_fault;
    }
    else if (info->is_present && info->is_exec && fault_vmap->vmflags & VM_EXEC && !(page_flags & VM_EXEC))
    {
        // vmprotect has been called on this vmap to enable execution
        // we need to make sure that the page is executable
        mm_do_flag(fault_vmap->mmctx->pgd, fault_addr, 1, page_flags | VM_EXEC);
        mm_unlock_ctx_pair(mm, NULL);
        spinlock_release(&fault_vmap->lock);
        return;
    }

    if (info->is_write && !(fault_vmap->vmflags & VM_WRITE))
    {
        unhandled_reason = "page fault in read-only vmap";
        mm_unlock_ctx_pair(mm, NULL);
        goto unhandled_fault;
    }

    if (info->is_present)
        info->faulting_page = pfn_phyframe(mm_do_get_pfn(fault_vmap->mmctx->pgd, fault_addr));

    static const char *const fault_result_names[] = {
        [VMFAULT_COMPLETE] = "COMPLETE",
        [VMFAULT_COPY_BACKING_PAGE] = "COPY_BACKING_PAGE",
        [VMFAULT_MAP_BACKING_PAGE] = "MAP_BACKING_PAGE",
        [VMFAULT_MAP_BACKING_PAGE_RO] = "MAP_BACKING_PAGE_RO",
        [VMFAULT_CANNOT_HANDLE] = "CANNOT_HANDLE",
    };

    mos_debug(cow, "handler %ps", (void *) (ptr_t) fault_vmap->on_fault);
    vmfault_result_t fault_result = fault_vmap->on_fault(fault_vmap, fault_addr, info);
    mos_debug_cont(cow, " -> %s (%d)", fault_result_names[fault_result], fault_result);

    vm_flags map_flags = fault_vmap->vmflags;
    switch (fault_result)
    {
        case VMFAULT_COMPLETE: break;
        case VMFAULT_CANNOT_HANDLE: break;
        case VMFAULT_COPY_BACKING_PAGE:
        {
            MOS_ASSERT(info->backing_page);
            const phyframe_t *page = mm_get_free_page(); // will be ref'd by mm_replace_page_locked()
            mm_copy_page(info->backing_page, page);
            info->backing_page = page;
            goto map_backing_page;
        }
        case VMFAULT_MAP_BACKING_PAGE_RO:
        {
            map_flags &= ~VM_WRITE;
            goto map_backing_page;
        }
        case VMFAULT_MAP_BACKING_PAGE:
        {
        map_backing_page:
            MOS_ASSERT(info->backing_page);
            mos_debug_cont(cow, " (backing page: " PFN_FMT ")", phyframe_pfn(info->backing_page));
            mm_replace_page_locked(fault_vmap->mmctx, fault_addr, phyframe_pfn(info->backing_page), map_flags);
            fault_result = VMFAULT_COMPLETE;
        }
    }

    MOS_ASSERT_X(fault_result == VMFAULT_COMPLETE || fault_result == VMFAULT_CANNOT_HANDLE, "invalid fault result %d", fault_result);
    spinlock_release(&fault_vmap->lock);
    mm_unlock_ctx_pair(mm, NULL);
    ipi_send_all(IPI_TYPE_INVALIDATE_TLB);
    if (fault_result == VMFAULT_COMPLETE)
        return;

// if we get here, the fault was not handled
unhandled_fault:
    MOS_ASSERT_X(unhandled_reason, "unhandled fault with no reason");
    invalid_page_fault(fault_addr, fault_vmap, info, unhandled_reason);
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
