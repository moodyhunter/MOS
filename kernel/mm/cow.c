// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/cow.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/task_type.h"
#include "mos/types.h"

// TODO: A global list of CoW blocks, so that we don't free them if they're still in use.

vmblock_t mm_map_cow(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages)
{
    vmblock_t block = mos_platform->mm_copy_maps(from, fvaddr, to, tvaddr, npages);
    mos_platform->mm_flag_pages(to, tvaddr, npages, block.flags & ~VM_WRITE);
    return block;
}

static void copy_cow_pages(uintptr_t vaddr, size_t npages)
{
    paging_handle_t current_page_handle = current_process->pagetable;
    void *pagetmp = kmalloc(MOS_PAGE_SIZE);
    for (size_t j = 0; j < npages; j++)
    {
        uintptr_t source_addr = vaddr + j * MOS_PAGE_SIZE;
        uintptr_t target_addr = vaddr + j * MOS_PAGE_SIZE;
        memcpy(pagetmp, (void *) source_addr, MOS_PAGE_SIZE);
        vm_flags flags = mos_platform->mm_get_flags(current_page_handle, source_addr);
        mos_platform->mm_unmap_pages(current_page_handle, source_addr, 1);
        mos_platform->mm_alloc_pages_at(current_page_handle, target_addr, 1, flags | VM_WRITE);
        memcpy((void *) target_addr, pagetmp, MOS_PAGE_SIZE);
    }
    kfree(pagetmp);
}

bool cow_handle_page_fault(uintptr_t fault_addr)
{
    if (!current_thread)
        return false;

    process_t *current_proc = current_thread->owner;

    for (ssize_t i = 0; i < current_proc->mmaps_count; i++)
    {
        const vmblock_t vm = current_proc->mmaps[i].vm;
        uintptr_t block_start = vm.vaddr;
        uintptr_t block_end = vm.vaddr + vm.npages * MOS_PAGE_SIZE;

        if (fault_addr < block_start || fault_addr >= block_end)
            continue;

        if (!current_proc->mmaps[i].cow_mapped)
            mos_panic("cow_handle_page_fault: page fault in non-cow mapped region");

        mos_debug("cow_handle_page_fault: fault_addr=" PTR_FMT ", vmblock=" PTR_FMT "-" PTR_FMT, fault_addr, vm.vaddr, vm.vaddr + vm.npages * MOS_PAGE_SIZE);

        copy_cow_pages(vm.vaddr, vm.npages);
        current_proc->mmaps[i].cow_mapped = false;
        return true;
    }

    return false;
}
