// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging.h"

#include "mos/mm/liballoc.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

void mm_setup()
{
    MOS_ASSERT(mos_platform.mm_page_size > 0);
    liballoc_init(mos_platform.mm_page_size);
}

void *mm_page_alloc(size_t npages)
{
    if (unlikely(npages <= 0))
    {
        mos_warn("allocating negative or zero pages");
        return NULL;
    }

    if (unlikely(mos_platform.mm_page_size == 0))
        mos_panic("platform configuration error: page_size is 0");

    if (unlikely(mos_platform.mm_alloc_page == NULL))
        mos_panic("platform configuration error: alloc_page is NULL");

    void *ptr = mos_platform.mm_alloc_page(npages);
    if (unlikely(ptr == NULL))
        mos_warn("failed to allocate %zu pages", npages);

    return ptr;
}

bool mm_page_free(void *ptr, size_t npages)
{
    if (unlikely(ptr == NULL))
    {
        mos_warn("freeing NULL pointer");
        return false;
    }
    if (unlikely(npages <= 0))
    {
        mos_warn("freeing negative or zero pages");
        return false;
    }
    if (unlikely(mos_platform.mm_free_page == NULL))
        mos_panic("platform configuration error: free_page is NULL");
    return mos_platform.mm_free_page(ptr, npages);
}
