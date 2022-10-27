// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging.h"

#include "mos/mm/liballoc.h"
#include "mos/mos_global.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/types.h"

void mos_kernel_mm_init()
{
    liballoc_init(MOS_PAGE_SIZE);
#if MOS_MM_LIBALLOC_DEBUG
    mos_install_kpanic_hook(liballoc_dump);
#endif
}

void *kheap_alloc_page(size_t npages, vm_flags vmflags)
{
    if (unlikely(npages <= 0))
    {
        mos_warn("allocating negative or zero pages");
        return NULL;
    }

    if (unlikely(mos_platform->mm_alloc_pages == NULL))
        mos_panic("platform configuration error: alloc_page is NULL");

    vmblock_t block = mos_platform->mm_alloc_pages(mos_platform->kernel_pg, npages, PGALLOC_HINT_KHEAP, vmflags);
    if (unlikely(block.pages < npages))
        mos_warn("failed to allocate %zu pages", npages);

    return (void *) block.vaddr;
}

bool kheap_free_page(void *vptr, size_t npages)
{
    MOS_ASSERT_X(vptr >= (void *) MOS_X86_HEAP_BASE_VADDR, "only use this function to free kernel heap pages");

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
    if (unlikely(mos_platform->mm_free_pages == NULL))
        mos_panic("platform configuration error: free_page is NULL");
    return mos_platform->mm_free_pages(mos_platform->kernel_pg, (uintptr_t) vptr, npages);
}
