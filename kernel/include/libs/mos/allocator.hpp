// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.hpp"

#include <mos/string_view.hpp>
#include <mos/type_utils.hpp>
#include <type_traits>

MOSAPI void *do_kmalloc(size_t size);
MOSAPI void *do_kcalloc(size_t nmemb, size_t size);
MOSAPI void *do_krealloc(void *ptr, size_t size);
MOSAPI void do_kfree(const void *ptr);

namespace mos
{
    template<typename T>
    concept HasTypeName = std::is_same_v<std::remove_const_t<decltype(T::type_name)>, mos::string_view>;

    template<typename T, typename... Args>
    requires HasTypeName<T> T *create(Args &&...args)
    {
        InitOnce<Slab<T>> slab;
        return slab->create(args...);
    }

    struct default_allocator
    {
        void *allocate(size_t size)
        {
            return do_kmalloc(size);
        }

        void free(void *ptr, size_t = 0)
        {
            do_kfree(ptr);
        }
    };
} // namespace mos
