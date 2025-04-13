// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/string_view.hpp>
#include <mos/type_utils.hpp>

MOSAPI void *do_kmalloc(size_t size);
MOSAPI void *do_kcalloc(size_t nmemb, size_t size);
MOSAPI void *do_krealloc(void *ptr, size_t size);
MOSAPI void do_kfree(const void *ptr);

namespace mos
{
    template<typename TItem, bool HasTypeName = HasTypeName<TItem>>
    struct default_allocator
    {
    };

    template<typename TItem>
    struct default_allocator<TItem, false>
    {
        static TItem *allocate(size_t size)
        {
            return static_cast<TItem *>(do_kmalloc(size));
        }

        static void free(TItem *ptr, size_t = 0)
        {
            do_kfree(ptr);
        }
    };

    template<typename TItem>
    struct default_allocator<TItem, true>
    {
        static TItem *allocate(size_t size)
        {
            return create<TItem>(size);
        }

        static void free(TItem *ptr, size_t = 0)
        {
            do_kfree(ptr);
        }
    };

} // namespace mos
