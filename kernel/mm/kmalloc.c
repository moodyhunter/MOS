// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/kmalloc.h"

#include "mos/mm/liballoc.h"

void *kmalloc(size_t size)
{
    return liballoc_malloc(size);
}

void *kcalloc(size_t nmemb, size_t size)
{
    return liballoc_calloc(nmemb, size);
}

void *krealloc(void *ptr, size_t size)
{
    return liballoc_realloc(ptr, size);
}

void kfree(void *ptr)
{
    liballoc_free(ptr);
}
