// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/liballoc.h"
#include "mos/types.h"

#include MOS_KERNEL_INTERNAL_HEADER_CHECK

should_inline __malloc void *kmalloc(size_t size)
{
    return liballoc_malloc(size);
}

should_inline __malloc void *kcalloc(size_t nmemb, size_t size)
{
    return liballoc_calloc(nmemb, size);
}

should_inline void *krealloc(void *ptr, size_t size)
{
    return liballoc_realloc(ptr, size);
}

should_inline void kfree(const void *ptr)
{
    liballoc_free(ptr);
}
