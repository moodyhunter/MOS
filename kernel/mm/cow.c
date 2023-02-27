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

vmblock_t mm_make_process_map_cow(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages)
{
    // Note that the block returned by this function contains it's ACTRUAL flags, which may be different from the flags of the original block.
    // (consider the case where we CoW-map a page that is already CoW-mapped)
    vmblock_t block = mm_copy_maps(from, fvaddr, to, tvaddr, npages);
    platform_mm_flag_pages(from, fvaddr, npages, block.flags & ~VM_WRITE);
    platform_mm_flag_pages(to, tvaddr, npages, block.flags & ~VM_WRITE);
    return block;
}

static vmblock_t copy_cow_pages_inplace(uintptr_t vaddr, size_t npages)
{
    paging_handle_t current_handle = current_process->pagetable;
    const vm_flags flags = platform_mm_get_flags(current_handle, vaddr); // this flags are the same for all pages (in the same proc_vmblock)
    const uintptr_t phys_start = pmalloc_alloc(npages);

    for (size_t j = 0; j < npages; j++)
    {
        vmblock_t stub_pblock = { .npages = 1, .flags = flags, .paddr = phys_start + j * MOS_PAGE_SIZE, .vaddr = vaddr + j * MOS_PAGE_SIZE };
        vmblock_t stub_vblock = mm_get_free_pages(current_handle, 1, PGALLOC_HINT_KHEAP);
        stub_pblock.vaddr = stub_vblock.vaddr;
        stub_pblock.flags |= VM_USER_RW;

        // firstly, map the phypage to a stub vaddr, then copy the data to there, then unmap the stub
        mm_map_allocated_pages(current_handle, stub_pblock);
        memcpy((void *) (vaddr + j * MOS_PAGE_SIZE), (void *) stub_pblock.vaddr, MOS_PAGE_SIZE);
        mm_unmap_pages(current_handle, stub_pblock.vaddr, 1);

        // then map the stub paddrs to the real vaddrs
        stub_pblock.vaddr = vaddr + j * MOS_PAGE_SIZE;
        mm_map_allocated_pages(current_handle, stub_pblock);
    }

    return (vmblock_t){ .flags = flags, .npages = npages, .paddr = phys_start, .vaddr = vaddr };
}

bool cow_handle_page_fault(uintptr_t fault_addr, bool present, bool is_write, bool is_user, bool is_exec)
{
    MOS_UNUSED(is_user);
    MOS_UNUSED(present);

    if (!current_thread)
        return false;

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

    for (ssize_t i = 0; i < current_proc->mmaps_count; i++)
    {
        proc_vmblock_t *mmap = &current_proc->mmaps[i];
        spinlock_acquire(&mmap->lock);

        const vmblock_t *vm = &mmap->blk;
        uintptr_t block_start = vm->vaddr;
        uintptr_t block_end = vm->vaddr + vm->npages * MOS_PAGE_SIZE;

        if (fault_addr < block_start || fault_addr >= block_end)
        {
            spinlock_release(&mmap->lock);
            continue;
        }

        const bool is_cow = mmap->flags & VMBLOCK_COW_ZERO_ON_DEMAND;
        const bool is_zero_on_demand = mmap->flags & VMBLOCK_COW_ZERO_ON_DEMAND;
        mos_debug(cow, "fault_addr=" PTR_FMT ", vmblock=" PTR_FMT "-" PTR_FMT, fault_addr, vm->vaddr, vm->vaddr + vm->npages * MOS_PAGE_SIZE);

        if (is_zero_on_demand)
        {
            pr_emph("Zero-on-demand page fault in block %ld", i);
            // TODO: only allocate the page where the fault happened
            mm_unmap_pages(current_proc->pagetable, vm->vaddr, vm->npages);
            mmap->blk = mm_alloc_pages_at(current_proc->pagetable, vm->vaddr, vm->npages, vm->flags);
            memzero((void *) vm->vaddr, MOS_PAGE_SIZE * vm->npages);
            mmap->flags &= ~VMBLOCK_COW_ZERO_ON_DEMAND;
            spinlock_release(&mmap->lock);
            mos_debug(cow, "ZoD resolved");
            return true;
        }
        else if (is_cow)
        {
            pr_emph("CoW page fault in block %ld", i);
            // TODO: only copy the page where the fault happened
            mmap->blk = copy_cow_pages_inplace(vm->vaddr, vm->npages);
            platform_mm_flag_pages(current_proc->pagetable, vm->vaddr, vm->npages, vm->flags);
            mmap->flags &= ~VMBLOCK_COW_COPY_ON_WRITE;
            spinlock_release(&mmap->lock);
            mos_debug(cow, "CoW resolved");
            return true;
        }
        else
        {
            pr_emerg("%s page fault (%s) in non-cow mapped region", is_user ? "User" : "Kernel", is_write ? "write" : "read");
            spinlock_release(&mmap->lock);
            return false;
        }
    }

    pr_emph("page fault in unmapped region: " PTR_FMT, fault_addr);
    return false;
}
