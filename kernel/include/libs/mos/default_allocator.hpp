// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/string_view.hpp>

MOSAPI void *do_kmalloc(size_t size);
MOSAPI void *do_kcalloc(size_t nmemb, size_t size);
MOSAPI void *do_krealloc(void *ptr, size_t size);
MOSAPI void do_kfree(const void *ptr);

namespace mos
{
    struct default_allocator
    {
        static void *allocate(size_t size)
        {
            return do_kmalloc(size);
        }

        static void free(void *ptr, size_t = 0)
        {
            do_kfree(ptr);
        }
    };
} // namespace mos
