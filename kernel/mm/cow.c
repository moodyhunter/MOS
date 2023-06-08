// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.h"

#include "mos/interrupt/ipi.h"
#include "mos/mm/paging/paging.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"

#include <string.h>

static vmblock_t zero_block;
static pfn_t zero_pfn;

void mm_cow_init(void)
{
    // zero fill on demand (read-only)
    mm_context_t *current_mm = current_cpu->mm_context;
    zero_block = mm_alloc_pages(current_mm, 1, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, VM_RW);
    zero_pfn = platform_mm_get_phys_addr(current_mm, zero_block.vaddr) / MOS_PAGE_SIZE;
    if (zero_block.vaddr == 0)
        mos_panic("failed to allocate zero block");
    memzero((void *) zero_block.vaddr, MOS_PAGE_SIZE);
    mm_flag_pages(current_mm, zero_block.vaddr, 1, VM_READ); // make it read-only after zeroing
}

vmblock_t mm_make_cow_block(mm_context_t *target_handle, vmblock_t src_block)
{
    return mm_make_cow(src_block.address_space, src_block.vaddr, target_handle, src_block.vaddr, src_block.npages, src_block.flags);
}

vmblock_t mm_make_cow(mm_context_t *from, ptr_t fvaddr, mm_context_t *to, ptr_t tvaddr, size_t npages, vm_flags flags)
{
    // do not use the return value of mm_copy_maps:
    // - the block returned by this function contains it's ACTUAL flags,
    // - which may be different from the flags of the original block,
    // - and we need the original flags to be able to resolve COW faults
    // (consider the case where we CoW-map a page that is already CoW-mapped)
    mm_flag_pages(from, fvaddr, npages, flags & ~VM_WRITE);

    vmblock_t block = mm_copy_maps(from, fvaddr, to, tvaddr, npages);
    // mm_flag_pages for newly-copied pages is not needed:
    // - mm_copy_maps already copies the flags, which has been modified by mm_flag_pages above
    block.flags = flags; // set the original flags
    return block;
}

vmblock_t mm_alloc_zeroed_pages(mm_context_t *mmctx, size_t npages, ptr_t vaddr, valloc_flags allocflags, vm_flags flags)
{
    const vm_flags ro_flags = VM_READ | ((flags & VM_USER) ? VM_USER : 0);

    spinlock_acquire(&mmctx->mm_lock);
    vaddr = mm_get_free_pages(mmctx, npages, vaddr, allocflags);

    // zero fill the pages
    for (size_t i = 0; i < npages; i++)
        mm_map_pages_locked(mmctx, vaddr + i * MOS_PAGE_SIZE, zero_pfn, 1, ro_flags);
    spinlock_release(&mmctx->mm_lock);

    return (vmblock_t){ .vaddr = vaddr, .npages = npages, .flags = flags, .address_space = mmctx };
}

static void do_resolve_cow(ptr_t fault_addr, vm_flags original_flags)
{
    fault_addr = ALIGN_DOWN_TO_PAGE(fault_addr);
    mm_context_t *mm = current_process->mm;

    // 1. create a new read-write page
    //    we are allocating the page in the kernel space
    //    so that user-space won't get confused by the new page
    const ptr_t proxy = mm_alloc_pages(mm, 1, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, VM_READ | VM_WRITE).vaddr;
    const pfn_t proxy_pfn = platform_mm_get_phys_addr(&platform_info->kernel_mm, proxy) / MOS_PAGE_SIZE;

    // 2. copy the data from the faulting address to the new page
    memcpy((void *) proxy, (void *) fault_addr, MOS_PAGE_SIZE);

    // 3. replace the faulting phypage with the new one
    //    this will increment the refcount of the new page, ...
    //    ...and also decrement the refcount of the old page
    mm_replace_mapping(mm, fault_addr, proxy_pfn, 1, original_flags);

    // 4. unmap the temporary page (at the kernel heap)
    mm_unmap_pages(mm, proxy, 1);

    ipi_send_all(IPI_TYPE_INVALIDATE_TLB);
}

bool mm_handle_pgfault(ptr_t fault_addr, bool present, bool is_write, bool is_user, bool is_exec)
{
    MOS_UNUSED(is_write);
    MOS_UNUSED(is_user);
    MOS_UNUSED(is_exec);

    if (!current_thread)
    {
        pr_warn("There shouldn't be a page fault before the scheduler is initialized!");
        return false;
    }

    if (!present)
    {
        // TODO file-backed mmap needs to be handled here
        return false;
    }

    process_t *current_proc = current_process;
    MOS_ASSERT_X(current_proc->mm == current_cpu->mm_context, "Page fault in a process that is not the current process?!");
#if MOS_DEBUG_FEATURE(cow)
    process_dump_mmaps(current_proc);
#endif

    process_t *const process = current_proc;
    list_foreach(vmap_t, mmap, process->mm->mmaps)
    {
        spinlock_acquire(&mmap->lock);

        const vmblock_t *const vm = &mmap->blk;
        const ptr_t block_start = vm->vaddr;
        const ptr_t block_end = vm->vaddr + vm->npages * MOS_PAGE_SIZE;

        if (fault_addr < block_start || fault_addr >= block_end)
        {
            spinlock_release(&mmap->lock);
            continue;
        }

        mos_debug(cow, "fault_addr=" PTR_FMT ", vmblock=" PTR_RANGE, fault_addr, block_start, block_end);

        if (!mmap->flags.cow)
        {
            pr_warn("Page fault in a non-COW block");
            spinlock_release(&mmap->lock);
            break;
        }

        // if the block is CoW, it must be writable
        MOS_ASSERT_X(mmap->blk.flags & VM_WRITE, "CoW fault in a non-writable block");
        do_resolve_cow(fault_addr, vm->flags);
        mos_debug(cow, "CoW resolved");

        spinlock_release(&mmap->lock);
        return true;
    }

    pr_emph("page fault in unmapped region: " PTR_FMT, fault_addr);
    return false;
}
