// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/memops.h"

#include "lib/liballoc.h"
#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/paging.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

static vmblock_t zero_block;

void mos_kernel_mm_init(void)
{
    // zero fill on demand (read-only)
    zero_block = mm_alloc_pages(current_cpu->pagetable, 1, PGALLOC_HINT_KHEAP, VM_RW);
    memzero((void *) zero_block.vaddr, MOS_PAGE_SIZE);
    mm_flag_pages(current_cpu->pagetable, zero_block.vaddr, 1, VM_READ); // make it read-only

    liballoc_init();
#if MOS_DEBUG_FEATURE(liballoc)
    declare_panic_hook(liballoc_dump);
    install_panic_hook(&liballoc_dump_holder);
#endif
#if MOS_DEBUG_FEATURE(pmm)
    declare_panic_hook(pmm_dump);
    install_panic_hook(&pmm_dump_holder);
#endif

    pmm_dump();
    pmm_switch_to_kheap();
}

// !! This function is called by liballoc, not intended to be called by anyone else !!
void *liballoc_alloc_page(size_t npages)
{
    if (unlikely(npages <= 0))
    {
        mos_warn("allocating negative or zero pages");
        return NULL;
    }

    vmblock_t block = mm_alloc_pages(current_cpu->pagetable, npages, PGALLOC_HINT_KHEAP, VM_RW);
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

    mm_unmap_pages(current_cpu->pagetable, (uintptr_t) vptr, npages);
    return true;
}

vmblock_t mm_alloc_zeroed_pages(paging_handle_t handle, size_t npages, pgalloc_hints hints, vm_flags flags)
{
    const uintptr_t vaddr_base = mm_get_free_pages(handle, npages, hints);
    return mm_alloc_zeroed_pages_at(handle, vaddr_base, npages, flags);
}

vmblock_t mm_alloc_zeroed_pages_at(paging_handle_t handle, uintptr_t vaddr, size_t npages, vm_flags flags)
{
    if (mm_get_is_mapped(handle, vaddr))
    {
        mos_warn("failed to allocate %zu pages at %p: already mapped", npages, (void *) vaddr);
        return (vmblock_t){ 0 };
    }

    // zero fill the pages
    for (size_t i = 0; i < npages; i++)
    {
        // actually, zero_block is always accessible, using [handle] == using [current_cpu->pagetable] as source
        mm_copy_mapping(current_cpu->pagetable, zero_block.vaddr, handle, vaddr + i * MOS_PAGE_SIZE, 1, MM_COPY_DEFAULT);
    }

    // make the pages read-only (because for now, they are mapped to zero_block)
    mm_flag_pages(handle, vaddr, npages, VM_READ | ((flags & VM_USER) ? VM_USER : 0));
    return (vmblock_t){ .vaddr = vaddr, .npages = npages, .flags = flags, .address_space = handle };
}
