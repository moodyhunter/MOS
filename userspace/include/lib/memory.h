// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/liballoc.h"
#include "mos/mos_global.h"

should_inline __malloc void *malloc(size_t size)
{
    return liballoc_malloc(size);
}
should_inline void free(void *ptr)
{
    liballoc_free(ptr);
}
should_inline __malloc void *calloc(size_t nmemb, size_t size)
{
    return liballoc_calloc(nmemb, size);
}
should_inline __malloc void *realloc(void *ptr, size_t size)
{
    return liballoc_realloc(ptr, size);
}
