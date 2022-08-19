// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/liballoc.h"
#include "mos/types.h"

always_inline __malloc void *kmalloc(size_t size)
{
    return liballoc_malloc(size);
}

always_inline __malloc void *kcalloc(size_t nmemb, size_t size)
{
    return liballoc_calloc(nmemb, size);
}

always_inline void *krealloc(void *ptr, size_t size)
{
    return liballoc_realloc(ptr, size);
}

always_inline void kfree(void *ptr)
{
    liballoc_free(ptr);
}
