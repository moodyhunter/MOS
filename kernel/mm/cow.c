// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"

// TODO: A global list of CoW blocks, so that we don't free them if they're still in use.

vmblock_t mm_make_process_map_cow(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages, vm_flags flags)
{
    // Note that the block returned by this function contains it's ACTRUAL flags, which may be different from the flags of the original block.
    // (consider the case where we CoW-map a page that is already CoW-mapped)
    mm_flag_pages(from, fvaddr, npages, flags & ~VM_WRITE);

    vmblock_t block = mm_copy_mapping(from, fvaddr, to, tvaddr, npages, MM_COPY_DEFAULT);
    mm_flag_pages(to, tvaddr, npages, flags & ~VM_WRITE);
    block.flags = flags;
    return block;
}

static void do_resolve_cow(uintptr_t fault_addr, vm_flags original_flags)
{
    fault_addr = ALIGN_DOWN_TO_PAGE(fault_addr);
    paging_handle_t address_space = current_process->pagetable;

    // 1. create a new read-write page
    vmblock_t one_page = mm_alloc_pages(address_space, 1, PGALLOC_HINT_KHEAP, original_flags);

    // 2. copy the data from the faulting address to the new page
    memcpy((void *) one_page.vaddr, (void *) fault_addr, MOS_PAGE_SIZE);

    // 3. replace the faulting phypage with the new one
    mm_copy_mapping(address_space, one_page.vaddr, address_space, fault_addr, 1, MM_COPY_ASSUME_MAPPED);

    // 4. unmap the temporary page (at the kernel heap)
    //    note at this point, the underlying physical page won't be freed, because it's still mapped to the faulting address
    mm_unmap_pages(address_space, one_page.vaddr, 1);
}

bool cow_handle_page_fault(uintptr_t fault_addr, bool present, bool is_write, bool is_user, bool is_exec)
{
    MOS_UNUSED(is_user);
    MOS_UNUSED(present);

    if (!current_thread)
    {
        pr_warn("There shouldn't be a page fault before the scheduler is initialized!");
        return false;
    }

    if (!is_write)
        return false;

    if (is_write && is_exec)
        mos_panic("Cannot write and execute at the same time");

    if (!present)
    {
        // TODO
        return false;
    }

    process_t *current_proc = current_process;
    MOS_ASSERT_X(current_proc->pagetable.pgd == current_cpu->pagetable.pgd, "Page fault in a process that is not the current process?!");
#if MOS_DEBUG_FEATURE(cow)
    process_dump_mmaps(current_proc);
#endif

    for (size_t i = 0; i < current_proc->mmaps_count; i++)
    {
        vmap_t *mmap = &current_proc->mmaps[i];
        spinlock_acquire(&mmap->lock);

        const vmblock_t *const vm = &mmap->blk;
        uintptr_t block_start = vm->vaddr;
        uintptr_t block_end = vm->vaddr + vm->npages * MOS_PAGE_SIZE;

        if (fault_addr < block_start || fault_addr >= block_end)
        {
            spinlock_release(&mmap->lock);
            continue;
        }

        mos_debug(cow, "fault_addr=" PTR_FMT ", vmblock=" PTR_FMT "-" PTR_FMT, fault_addr, vm->vaddr, vm->vaddr + vm->npages * MOS_PAGE_SIZE);

        if (mmap->flags.zod || mmap->flags.cow)
        {
            pr_emph("CoW page fault in block %zu", i);
            do_resolve_cow(fault_addr, vm->flags);
            mos_debug(cow, "CoW resolved");
        }
        else
        {
            pr_warn("Page fault in a non-COW block (block %zu)", i);
            spinlock_release(&mmap->lock);
            return false;
        }

        spinlock_release(&mmap->lock);
        return true;
    }

    pr_emph("page fault in unmapped region: " PTR_FMT, fault_addr);
    return false;
}
