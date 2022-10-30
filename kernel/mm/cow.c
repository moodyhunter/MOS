// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/task_type.h"

// TODO: A global list of CoW blocks, so that we don't free them if they're still in use.

vmblock_t mm_make_process_map_cow(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages)
{
    vmblock_t block = mos_platform->mm_copy_maps(from, fvaddr, to, tvaddr, npages);
    mos_platform->mm_flag_pages(from, fvaddr, npages, block.flags & ~VM_WRITE);
    mos_platform->mm_flag_pages(to, tvaddr, npages, block.flags & ~VM_WRITE);
    return block;
}

void copy_cow_pages_inplace(uintptr_t vaddr, size_t npages)
{
    paging_handle_t current_page_handle = current_process->pagetable;
    void *pagetmp = kmalloc(MOS_PAGE_SIZE);
    for (size_t j = 0; j < npages; j++)
    {
        const uintptr_t current_page = vaddr + j * MOS_PAGE_SIZE;
        memcpy(pagetmp, (void *) current_page, MOS_PAGE_SIZE);
        vm_flags flags = mos_platform->mm_get_flags(current_page_handle, current_page);
        mos_platform->mm_unmap_pages(current_page_handle, current_page, 1);
        mos_platform->mm_alloc_pages_at(current_page_handle, current_page, 1, flags | VM_WRITE);
        memcpy((void *) current_page, pagetmp, MOS_PAGE_SIZE);
    }
    kfree(pagetmp);
}

bool cow_handle_page_fault(uintptr_t fault_addr, bool present, bool is_write, bool is_user, bool is_exec)
{
    MOS_UNUSED(is_user);
    MOS_UNUSED(is_exec);

    if (!current_thread)
        return false;

    if (!present || !is_write)
        return false;

    process_t *current_proc = current_process;

    for (ssize_t i = 0; i < current_proc->mmaps_count; i++)
    {
        const vmblock_t vm = current_proc->mmaps[i].vm;
        uintptr_t block_start = vm.vaddr;
        uintptr_t block_end = vm.vaddr + vm.npages * MOS_PAGE_SIZE;

        if (fault_addr < block_start || fault_addr >= block_end)
            continue;

        if (!(current_proc->mmaps[i].map_flags & MMAP_COW))
            mos_panic("page fault in non-cow mapped region");

        mos_debug("fault_addr=" PTR_FMT ", vmblock=" PTR_FMT "-" PTR_FMT, fault_addr, vm.vaddr, vm.vaddr + vm.npages * MOS_PAGE_SIZE);

        copy_cow_pages_inplace(vm.vaddr, vm.npages);
        mos_platform->mm_flag_pages(current_proc->pagetable, vm.vaddr, vm.npages, vm.flags);
        current_proc->mmaps[i].map_flags &= ~MMAP_COW;
        return true;
    }

    pr_emph("page fault in unmapped region: " PTR_FMT, fault_addr);
    return false;
}
