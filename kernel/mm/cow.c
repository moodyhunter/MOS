// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/interrupt/ipi.h>
#include <mos/mm/cow.h>
#include <mos/mm/kmalloc.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <string.h>

static vmblock_t zero_block;
static ptr_t zero_paddr;

void mm_cow_init(void)
{
    // zero fill on demand (read-only)
    zero_block = mm_alloc_pages(current_cpu->pagetable, 1, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, VM_RW);
    memzero((void *) zero_block.vaddr, MOS_PAGE_SIZE);
    mm_flag_pages(current_cpu->pagetable, zero_block.vaddr, 1, VM_READ); // make it read-only after zeroing
    zero_paddr = platform_mm_get_phys_addr(current_cpu->pagetable, zero_block.vaddr);
}

vmblock_t mm_make_cow_block(paging_handle_t target_handle, vmblock_t src_block)
{
    return mm_make_cow(src_block.address_space, src_block.vaddr, target_handle, src_block.vaddr, src_block.npages, src_block.flags);
}

vmblock_t mm_make_cow(paging_handle_t from, ptr_t fvaddr, paging_handle_t to, ptr_t tvaddr, size_t npages, vm_flags flags)
{
    // Note that the block returned by this function contains it's ACTRUAL flags, which may be different from the flags of the original block.
    // (consider the case where we CoW-map a page that is already CoW-mapped)
    mm_flag_pages(from, fvaddr, npages, flags & ~VM_WRITE);

    vmblock_t block = mm_copy_maps(from, fvaddr, to, tvaddr, npages, MM_COPY_DEFAULT);
    mm_flag_pages(to, tvaddr, npages, flags & ~VM_WRITE);
    block.flags = flags;
    return block;
}

vmblock_t mm_alloc_zeroed_pages(paging_handle_t handle, size_t npages, ptr_t vaddr, valloc_flags allocation_flags, vm_flags flags)
{
    vaddr = mm_get_free_pages(handle, npages, vaddr, allocation_flags);

    // zero fill the pages
    for (size_t i = 0; i < npages; i++)
        mm_fill_pages(handle, vaddr + i * MOS_PAGE_SIZE, zero_paddr, 1, VM_READ | ((flags & VM_USER) ? VM_USER : 0));

    // make the pages read-only (because for now, they are mapped to zero_block)
    return (vmblock_t){ .vaddr = vaddr, .npages = npages, .flags = flags, .address_space = handle };
}

static void do_resolve_cow(ptr_t fault_addr, vm_flags original_flags)
{
    fault_addr = ALIGN_DOWN_TO_PAGE(fault_addr);
    paging_handle_t current_handle = current_process->pagetable;

    // 1. create a new read-write page
    vmblock_t one_page = mm_alloc_pages(current_handle, 1, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, original_flags);

    // 2. copy the data from the faulting address to the new page
    memcpy((void *) one_page.vaddr, (void *) fault_addr, MOS_PAGE_SIZE);

    // 3. replace the faulting phypage with the new one
    // mm_copy_maps(current_handle, one_page.vaddr, current_handle, fault_addr, 1, MM_COPY_REMAP);
    const ptr_t new_paddr = platform_mm_get_phys_addr(platform_info->kernel_pgd, one_page.vaddr);
    const ptr_t current_paddr = platform_mm_get_phys_addr(current_handle, fault_addr);

    pmm_ref_frames(new_paddr, 1);
    spinlock_acquire(current_handle.pgd_lock);
    platform_mm_map_pages(current_handle, fault_addr, new_paddr, 1, one_page.flags);
    spinlock_release(current_handle.pgd_lock);
    pmm_unref_frames(current_paddr, 1);

    // 4. unmap the temporary page (at the kernel heap)
    //    note at this point, the underlying physical page won't be freed, because it's still mapped to the faulting address
    mm_unmap_pages(current_handle, one_page.vaddr, 1);

    ipi_send_all(IPI_TYPE_INVALIDATE_TLB);
}

bool mm_handle_pgfault(ptr_t fault_addr, bool present, bool is_write, bool is_user, bool is_exec)
{
    MOS_UNUSED(is_user);
    MOS_UNUSED(present);

    if (!current_thread)
    {
        pr_warn("There shouldn't be a page fault before the scheduler is initialized!");
        return false;
    }

    if (!is_write)
        return false; // we only handle write faults

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
        ptr_t block_start = vm->vaddr;
        ptr_t block_end = vm->vaddr + vm->npages * MOS_PAGE_SIZE;

        if (fault_addr < block_start || fault_addr >= block_end)
        {
            spinlock_release(&mmap->lock);
            continue;
        }

        mos_debug(cow, "fault_addr=" PTR_FMT ", vmblock=" PTR_RANGE, fault_addr, vm->vaddr, vm->vaddr + vm->npages * MOS_PAGE_SIZE);

        if (mmap->flags.cow)
        {
            MOS_ASSERT(mmap->blk.flags & VM_WRITE); // if the block is CoW, it must be writable
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
