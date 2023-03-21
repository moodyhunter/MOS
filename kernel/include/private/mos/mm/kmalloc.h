// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "liballoc.h"

#include <mos/types.h>

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

should_inline void *kzalloc(size_t size)
{
    return kcalloc(1, size);
}
