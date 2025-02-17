// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.hpp"

#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/interrupt/ipi.hpp"
#include "mos/misc/setup.hpp"
#include "mos/mm/paging/paging.hpp"
#include "mos/mm/paging/pmlx/pml5.hpp"
#include "mos/mm/paging/table_ops.hpp"
#include "mos/mm/physical/pmm.hpp"
#include "mos/platform/platform.hpp"
#include "mos/platform/platform_defs.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/signal.hpp"

#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/mos_global.h>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

#if MOS_CONFIG(MOS_MM_DETAILED_MMAPS_UNHANDLED_FAULT)
#include "mos/tasks/process.hpp"
#endif

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

MMContext *mm_create_context(void)
{
    MMContext *mmctx = mos::create<MMContext>();
    linked_list_init(&mmctx->mmaps);

    pml4_t pml4 = pml_create_table(pml4);

    // map the upper half of the address space to the kernel
    for (int i = pml4_index(MOS_KERNEL_START_VADDR); i < PML4_ENTRIES; i++)
        pml4.table[i] = platform_info->kernel_mm->pgd.max.next.table[i];

    mmctx->pgd = pgd_create(pml4);

    return mmctx;
}

void mm_destroy_context(MMContext *mmctx)
{
    MOS_ASSERT(mmctx != platform_info->kernel_mm); // you can't destroy the kernel mmctx
    MOS_ASSERT(list_is_empty(&mmctx->mmaps));

    ptr_t zero = 0;
    size_t userspace_npages = (MOS_USER_END_VADDR + 1) / MOS_PAGE_SIZE;
    const bool freed = pml5_destroy_range(mmctx->pgd.max, &zero, &userspace_npages);
    MOS_ASSERT_X(freed, "failed to free the entire userspace");
    delete mmctx;
}

void mm_lock_ctx_pair(MMContext *ctx1, MMContext *ctx2)
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

void mm_unlock_ctx_pair(MMContext *ctx1, MMContext *ctx2)
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

MMContext *mm_switch_context(MMContext *new_ctx)
{
    MMContext *old_ctx = current_cpu->mm_context;
    if (old_ctx == new_ctx)
        return old_ctx;

    platform_switch_mm(new_ctx);
    current_cpu->mm_context = new_ctx;
    return old_ctx;
}

static void do_attach_vmap(MMContext *mmctx, vmap_t *vmap)
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

vmap_t *vmap_create(MMContext *mmctx, ptr_t vaddr, size_t npages)
{
    MOS_ASSERT_X(mmctx != platform_info->kernel_mm, "you can't create vmaps in the kernel mmctx");
    vmap_t *map = mos::create<vmap_t>();
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
    MMContext *const mm = vmap->mmctx;
    MOS_ASSERT(spinlock_is_locked(&mm->mm_lock));
    if (vmap->io)
    {
        bool unmapped = false;
        if (!io_munmap(vmap->io, vmap, &unmapped))
            pr_warn("munmap: could not unmap the file: io_munmap() failed");

        if (unmapped)
            goto unmapped;
    }
    mm_do_unmap(mm->pgd, vmap->vaddr, vmap->npages, true);

unmapped:
    list_remove(vmap);
    delete vmap;
}

vmap_t *vmap_obtain(MMContext *mmctx, ptr_t vaddr, size_t *out_offset)
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

    vmap_t *second = mos::create<vmap_t>();
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

static void invalid_page_fault(ptr_t fault_addr, vmap_t *faulting_vmap, vmap_t *ip_vmap, pagefault_t *info, const char *unhandled_reason)
{
    pr_emerg("unhandled page fault: %s", unhandled_reason);
#if MOS_CONFIG(MOS_MM_DETAILED_UNHANDLED_FAULT)
    pr_emerg("  invalid %s mode %s %s page [" PTR_FMT "]",                               //
             info->is_user ? "user" : "kernel",                                          //
             info->is_write ? "write to" : (info->is_exec ? "execute in" : "read from"), //
             info->is_present ? "present" : "non-present",                               //
             fault_addr                                                                  //
    );

    pr_emerg("  instruction: " PTR_FMT, info->ip);
    if (ip_vmap)
    {
        pr_emerg("    vmap: %pvm", (void *) ip_vmap);
        pr_emerg("    offset: 0x%zx", info->ip - ip_vmap->vaddr + (ip_vmap->io ? ip_vmap->io_offset : 0));
    }

    pr_emerg("    thread: %pt", current_thread);
    pr_emerg("    process: %pp", current_thread ? current_process : nullptr);

    if (fault_addr < 1 KB)
    {
        if (info->is_write)
            pr_emerg("  possible write to NULL pointer");
        else if (info->is_exec && fault_addr == 0)
            pr_emerg("  attempted to execute NULL pointer");
        else
            pr_emerg("  possible NULL pointer dereference");
    }

    if (info->is_user && fault_addr > MOS_KERNEL_START_VADDR)
        pr_emerg("    kernel address dereference");

    if (info->ip > MOS_KERNEL_START_VADDR)
        pr_emerg("    in kernel function %ps", (void *) info->ip);

    if (faulting_vmap)
    {
        pr_emerg("    in vmap: %pvm", (void *) faulting_vmap);
        pr_emerg("       offset: 0x%zx", fault_addr - faulting_vmap->vaddr + (faulting_vmap->io ? faulting_vmap->io_offset : 0));
    }

    if (faulting_vmap)
        spinlock_release(&faulting_vmap->lock);

    if (ip_vmap)
        spinlock_release(&ip_vmap->lock);

    if (current_thread)
        spinlock_release(&current_thread->owner->mm->mm_lock);

#if MOS_CONFIG(MOS_MM_DETAILED_MMAPS_UNHANDLED_FAULT)
    if (current_thread)
        process_dump_mmaps(current_process);
#endif

    pr_info("stack trace before fault (may be unreliable):");
    platform_dump_stack(info->regs);

    pr_info("register states before fault:");
    platform_dump_regs(info->regs);
    pr_cont("\n");
#else
    MOS_UNUSED(fault_addr);
    MOS_UNUSED(info);
#endif

    if (current_thread)
    {
        signal_send_to_thread(current_thread, SIGSEGV);
        signal_exit_to_user_prepare(info->regs);
    }
    else
    {
        MOS_ASSERT(!"unhandled kernel page fault");
    }
}

void mm_handle_fault(ptr_t fault_addr, pagefault_t *info)
{
    const char *unhandled_reason = NULL;

    pr_demph(pagefault, "%s #PF: %pt, %pp, IP=" PTR_VLFMT ", ADDR=" PTR_VLFMT, //
             info->is_user ? "user" : "kernel",                                //
             current_thread ? current_thread : NULL,                           //
             current_thread ? current_thread->owner : NULL,                    //
             info->ip,                                                         //
             fault_addr                                                        //
    );

    if (info->is_write && info->is_exec)
        mos_panic("Cannot write and execute at the same time");

    size_t offset = 0;
    vmap_t *fault_vmap = NULL;
    vmap_t *ip_vmap = NULL;

    const auto DoUnhandledPageFault = [&]()
    {
        // if we get here, the fault was not handled
        MOS_ASSERT_X(unhandled_reason, "unhandled fault with no reason");
        invalid_page_fault(fault_addr, fault_vmap, ip_vmap, info, unhandled_reason);
    };

    if (!current_mm)
    {
        unhandled_reason = "no mm context";
        DoUnhandledPageFault();
        return;
    }

    MMContext *const mm = current_mm;
    mm_lock_ctx_pair(mm, NULL);

    fault_vmap = vmap_obtain(mm, fault_addr, &offset);
    if (!fault_vmap)
    {
        ip_vmap = vmap_obtain(mm, info->ip, NULL);
        unhandled_reason = "page fault in unmapped area";
        mm_unlock_ctx_pair(mm, NULL);
        DoUnhandledPageFault();
        return;
    }
    ip_vmap = MOS_IN_RANGE(info->ip, fault_vmap->vaddr, fault_vmap->vaddr + fault_vmap->npages * MOS_PAGE_SIZE) ? fault_vmap : vmap_obtain(mm, info->ip, NULL);

    MOS_ASSERT_X(fault_vmap->on_fault, "vmap %pvm has no fault handler", (void *) fault_vmap);
    const vm_flags page_flags = mm_do_get_flags(fault_vmap->mmctx->pgd, fault_addr);

    if (info->is_exec && !(fault_vmap->vmflags & VM_EXEC))
    {
        unhandled_reason = "page fault in non-executable vmap";
        mm_unlock_ctx_pair(mm, NULL);
        DoUnhandledPageFault();
        return;
    }
    else if (info->is_present && info->is_exec && fault_vmap->vmflags & VM_EXEC && !(page_flags & VM_EXEC))
    {
        // vmprotect has been called on this vmap to enable execution
        // we need to make sure that the page is executable
        mm_do_flag(fault_vmap->mmctx->pgd, fault_addr, 1, page_flags | VM_EXEC);
        mm_unlock_ctx_pair(mm, NULL);
        spinlock_release(&fault_vmap->lock);
        if (ip_vmap)
            spinlock_release(&ip_vmap->lock);
        return;
    }

    if (info->is_write && !(fault_vmap->vmflags & VM_WRITE))
    {
        unhandled_reason = "page fault in read-only vmap";
        mm_unlock_ctx_pair(mm, NULL);
        DoUnhandledPageFault();
        return;
    }

    if (info->is_present)
        info->faulting_page = pfn_phyframe(mm_do_get_pfn(fault_vmap->mmctx->pgd, fault_addr));

    const auto get_fault_result = [](vmfault_result_t result)
    {
        switch (result)
        {
            case VMFAULT_COMPLETE: return "COMPLETE";
            case VMFAULT_MAP_BACKING_PAGE_RO: return "MAP_BACKING_PAGE_RO";
            case VMFAULT_MAP_BACKING_PAGE: return "MAP_BACKING_PAGE";
            case VMFAULT_COPY_BACKING_PAGE: return "COPY_BACKING_PAGE";
            case VMFAULT_CANNOT_HANDLE: return "CANNOT_HANDLE";
            default: return "UNKNOWN";
        };
    };

    pr_dcont(pagefault, ", handler %ps", (void *) (ptr_t) fault_vmap->on_fault);
    vmfault_result_t fault_result = fault_vmap->on_fault(fault_vmap, fault_addr, info);
    pr_dcont(pagefault, " -> %s", get_fault_result(fault_result));

    vm_flags map_flags = fault_vmap->vmflags;
    switch (fault_result)
    {
        case VMFAULT_COMPLETE: break;
        case VMFAULT_CANNOT_HANDLE:
        {
            unhandled_reason = "vmap fault handler returned VMFAULT_CANNOT_HANDLE";
            DoUnhandledPageFault();
            return;
        }
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
            if (!info->backing_page)
            {
                unhandled_reason = "out of memory";
                mm_unlock_ctx_pair(mm, NULL);
                DoUnhandledPageFault();
                return;
            }

            pr_dcont(pagefault, " (backing page: " PFN_FMT ")", phyframe_pfn(info->backing_page));
            mm_replace_page_locked(fault_vmap->mmctx, fault_addr, phyframe_pfn(info->backing_page), map_flags);
            fault_result = VMFAULT_COMPLETE;
        }
    }

    MOS_ASSERT_X(fault_result == VMFAULT_COMPLETE || fault_result == VMFAULT_CANNOT_HANDLE, "invalid fault result %d", fault_result);
    if (ip_vmap)
        spinlock_release(&ip_vmap->lock);
    spinlock_release(&fault_vmap->lock);
    mm_unlock_ctx_pair(mm, NULL);
    ipi_send_all(IPI_TYPE_INVALIDATE_TLB);
    if (fault_result == VMFAULT_COMPLETE)
        return;

    DoUnhandledPageFault();
}

// ! sysfs support

static bool sys_mem_mmap(sysfs_file_t *f, vmap_t *vmap, off_t offset)
{
    MOS_UNUSED(f);
    // pr_info("mem: mapping " PTR_VLFMT " to " PTR_VLFMT "\n", vmap->vaddr, offset);
    mm_do_map(vmap->mmctx->pgd, vmap->vaddr, offset / MOS_PAGE_SIZE, vmap->npages, vmap->vmflags, false);
    return true;
}

static bool sys_mem_munmap(sysfs_file_t *f, vmap_t *vmap, bool *unmapped)
{
    MOS_UNUSED(f);
    mm_do_unmap(vmap->mmctx->pgd, vmap->vaddr, vmap->npages, false);
    *unmapped = true;
    return true;
}

static sysfs_item_t sys_mem_item = SYSFS_MEM_ITEM("mem", sys_mem_mmap, sys_mem_munmap);

static void mm_sysfs_init()
{
    sys_mem_item.mem.size = platform_info->max_pfn * MOS_PAGE_SIZE;
    sysfs_register_root_file(&sys_mem_item);
}

MOS_INIT(SYSFS, mm_sysfs_init);
