// SPDX-License-Identifier: GPL-3.0-or-later

#include "liballoc.h"

#include <mos/mm/kmalloc.h>
#include <mos/mm/memops.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <string.h>

static vmblock_t zero_block;
static ptr_t zero_paddr;

void mos_kernel_mm_init(void)
{
    // zero fill on demand (read-only)
    zero_block = mm_alloc_pages(current_cpu->pagetable, 1, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, VM_RW);
    memzero((void *) zero_block.vaddr, MOS_PAGE_SIZE);
    mm_flag_pages(current_cpu->pagetable, zero_block.vaddr, 1, VM_READ); // make it read-only after zeroing
    zero_paddr = platform_mm_get_phys_addr(current_cpu->pagetable, zero_block.vaddr);

    liballoc_init();
#if MOS_DEBUG_FEATURE(liballoc)
    declare_panic_hook(liballoc_dump);
    install_panic_hook(&liballoc_dump_holder);
#endif
#if MOS_DEBUG_FEATURE(pmm)
    declare_panic_hook(pmm_dump_lists);
    install_panic_hook(&pmm_dump_lists_holder);
#endif

    pmm_dump_lists();
    pmm_switch_to_kheap();
}

// !! This function is called by liballoc, not intended to be called by anyone else !!
void *liballoc_alloc_page(size_t npages)
{
    const vmblock_t block = mm_alloc_pages(current_cpu->pagetable, npages, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, VM_RW);
    if (block.vaddr == 0)
        return NULL;

    MOS_ASSERT_X(block.vaddr >= MOS_ADDR_KERNEL_HEAP, "only use this function to free kernel heap pages");

    if (unlikely(block.npages < npages))
        mos_warn("failed to allocate %zu pages", npages);

    return (void *) block.vaddr;
}

// !! This function is called by liballoc, not intended to be called by anyone else !!
bool liballoc_free_page(void *vptr, size_t npages)
{
    MOS_ASSERT_X(vptr >= (void *) MOS_ADDR_KERNEL_HEAP, "only use this function to free kernel heap pages");

    if (unlikely(vptr == NULL))
    {
        mos_warn("freeing NULL pointer");
        return false;
    }
    if (unlikely(npages <= 0))
    {
        mos_warn("freeing negative or zero pages");
        return false;
    }

    mm_unmap_pages(current_cpu->pagetable, (ptr_t) vptr, npages);
    return true;
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
