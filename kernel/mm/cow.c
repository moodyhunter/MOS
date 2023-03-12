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

static vmblock_t do_resolve_cow(uintptr_t fault_addr, size_t npages)
{
    paging_handle_t current_handle = current_process->pagetable;
    const vm_flags flags = platform_mm_get_flags(current_handle, fault_addr) | VM_USER_RW; // this flags are the same for all pages (in the same proc_vmblock)
    pmblock_t *pmblocks = pmm_allocate(npages);

    size_t page_i = 0;

    list_headless_foreach(pmblock_t, pm, add_const(*pmblocks))
    {
        pmblock_t stub_pblock = {
            .list_node = LIST_NODE_INIT(stub_pblock),
            .paddr = pm->paddr,
            .npages = pm->npages,
        };

        vmblock_t stub_vblock = {
            .vaddr = mm_get_free_pages(current_handle, pm->npages, PGALLOC_HINT_KHEAP).vaddr,
            .flags = flags,
            .npages = pm->npages,
            .pblocks = &stub_pblock,
        };

        const uintptr_t current_fault_addr = fault_addr + page_i * MOS_PAGE_SIZE;

        mm_map_allocated_pages(current_handle, stub_vblock);                                         // 1. firstly, map the phypage to a [stub vaddr]
        memcpy((void *) stub_vblock.vaddr, (void *) current_fault_addr, pm->npages * MOS_PAGE_SIZE); // 2. copy the data to the stub
        mm_unmap_pages(current_handle, stub_vblock.vaddr, pm->npages);                               // 3. unmap the stub
        mm_unmap_pages(current_handle, current_fault_addr, pm->npages);                              // 4. unmap the [real vaddr], so that we can map the stub there
        stub_vblock.vaddr = current_fault_addr;                                                      // 5. replace the [stub vaddr] with the [real vaddr]
        mm_map_allocated_pages(current_handle, stub_vblock);                                         // 6. map the stub to the [real vaddr]

        page_i += stub_vblock.npages;
    }

    return (vmblock_t){ .flags = flags, .npages = npages, .pblocks = pmblocks, .vaddr = fault_addr };
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
