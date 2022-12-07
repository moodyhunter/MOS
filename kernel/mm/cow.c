// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_type.h"

// TODO: A global list of CoW blocks, so that we don't free them if they're still in use.

vmblock_t mm_make_process_map_cow(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages)
{
    // Note that the block returned by this function contains it's ACTRUAL flags, which may be different from the flags of the original block.
    // (consider the case where we CoW-map a page that is already CoW-mapped)
    vmblock_t block = platform_mm_copy_maps(from, fvaddr, to, tvaddr, npages);
    platform_mm_flag_pages(from, fvaddr, npages, block.flags & ~VM_WRITE);
    platform_mm_flag_pages(to, tvaddr, npages, block.flags & ~VM_WRITE);
    return block;
}

static void copy_cow_pages_inplace(uintptr_t vaddr, size_t npages)
{
    paging_handle_t pg_handle = current_process->pagetable;
    const vm_flags flags = platform_mm_get_flags(pg_handle, vaddr); // this flags are the same for all pages (in the same proc_vmblock)
    void *pagetmp = kmalloc(MOS_PAGE_SIZE);
    for (size_t j = 0; j < npages; j++)
    {
        const uintptr_t current_page = vaddr + j * MOS_PAGE_SIZE;
        bool mapped = platform_mm_get_is_mapped(pg_handle, current_page);
        if (!mapped)
        {
            // in the case where this page is not mapped, it means the original page was "made-up"
            // (consider the case where user heap is grown immediately after a fork)
            // we just allocate a new page, (since there's no 'original' page to copy from)
            platform_mm_alloc_pages_at(pg_handle, current_page, 1, flags);
            continue;
        }
        memcpy(pagetmp, (void *) current_page, MOS_PAGE_SIZE);
        platform_mm_unmap_pages(pg_handle, current_page, 1);
        platform_mm_alloc_pages_at(pg_handle, current_page, 1, flags | VM_WRITE);
        memcpy((void *) current_page, pagetmp, MOS_PAGE_SIZE);
    }
    kfree(pagetmp);
}

bool cow_handle_page_fault(uintptr_t fault_addr, bool present, bool is_write, bool is_user, bool is_exec)
{
    MOS_UNUSED(is_user);
    MOS_UNUSED(present);

    if (!current_thread)
        return false;

    if (!is_write)
        return false;

    if (is_write && is_exec)
        mos_panic("Cannot write and execute at the same time");

    process_t *current_proc = current_process;
    process_dump_mmaps(current_proc);

    for (ssize_t i = 0; i < current_proc->mmaps_count; i++)
    {
        const vmblock_t vm = current_proc->mmaps[i].vm;
        uintptr_t block_start = vm.vaddr;
        uintptr_t block_end = vm.vaddr + vm.npages * MOS_PAGE_SIZE;

        if (fault_addr < block_start || fault_addr >= block_end)
            continue;

        if (!(current_proc->mmaps[i].map_flags & MMAP_COW))
            mos_panic("%s page fault (%s) in non-cow mapped region", is_user ? "User" : "Kernel", is_write ? "write" : "read");

        pr_info2("CoW page fault in block %ld", i);
        mos_debug("fault_addr=" PTR_FMT ", vmblock=" PTR_FMT "-" PTR_FMT, fault_addr, vm.vaddr, vm.vaddr + vm.npages * MOS_PAGE_SIZE);

        copy_cow_pages_inplace(vm.vaddr, vm.npages);
        platform_mm_flag_pages(current_proc->pagetable, vm.vaddr, vm.npages, vm.flags);
        current_proc->mmaps[i].map_flags &= ~MMAP_COW;
        return true;
    }

    pr_emph("page fault in unmapped region: " PTR_FMT, fault_addr);
    return false;
}
