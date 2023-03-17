// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pmalloc.h"
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

    vmblock_t block = mm_copy_maps(from, fvaddr, to, tvaddr, npages, MM_COPY_DEFAULT);
    mm_flag_pages(to, tvaddr, npages, flags & ~VM_WRITE);
    block.flags = flags;
    return block;
}

static vmblock_t do_resolve_cow(uintptr_t fault_addr, size_t npages)
{
    paging_handle_t current_handle = current_process->pagetable;
    const vm_flags flags = platform_mm_get_flags(current_handle, fault_addr) | VM_USER_RW; // this flags are the same for all pages (in the same proc_vmblock)
    const uintptr_t phys_start = pmalloc_alloc(npages);

    for (size_t j = 0; j < npages; j++)
    {
        vmblock_t stub_pblock = {
            .vaddr = mm_get_free_pages(current_handle, 1, PGALLOC_HINT_KHEAP),
            .paddr = phys_start + j * MOS_PAGE_SIZE,
            .flags = flags,
            .npages = 1,
        };

        // firstly, map the phypage to a stub vaddr
        // then copy the data to the stub
        // then unmap the stub
        mm_map_allocated_pages(current_handle, stub_pblock);
        memcpy((void *) stub_pblock.vaddr, (void *) (fault_addr + j * MOS_PAGE_SIZE), MOS_PAGE_SIZE);
        mm_unmap_pages(current_handle, stub_pblock.vaddr, 1); // unmap the stub

        stub_pblock.vaddr = fault_addr + j * MOS_PAGE_SIZE;   // replace the stub vaddr with the real vaddr
        mm_unmap_pages(current_handle, stub_pblock.vaddr, 1); // unmap the real vaddr, so that we can map the stub there
        mm_map_allocated_pages(current_handle, stub_pblock);
    }

    return (vmblock_t){ .flags = flags, .npages = npages, .paddr = phys_start, .vaddr = fault_addr, .address_space = current_handle };
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

    if (!present)
        return false;

    if (!is_write)
        return false;

    if (is_write && is_exec)
        mos_panic("Cannot write and execute at the same time");

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

        if (mmap->flags.zod)
        {
            pr_emph("Zero-on-demand page fault in block %zu", i);
            // TODO: only allocate the page where the fault happened
            mm_unmap_pages(current_proc->pagetable, vm->vaddr, vm->npages);
            mmap->blk = mm_alloc_pages_at(current_proc->pagetable, vm->vaddr, vm->npages, vm->flags);
            memzero((void *) vm->vaddr, MOS_PAGE_SIZE * vm->npages);
            mos_debug(cow, "ZoD resolved");
            mmap->flags.zod = false;
        }
        else if (mmap->flags.cow)
        {
            pr_emph("CoW page fault in block %zu", i);
            // TODO: only copy the page where the fault happened
            mmap->blk = do_resolve_cow(vm->vaddr, vm->npages);
            mos_debug(cow, "CoW resolved");
            mmap->flags.cow = false;
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
