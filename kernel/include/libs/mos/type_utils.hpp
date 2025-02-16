// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <mos/string_view.hpp>

namespace mos
{
    template<auto n>
    struct string_literal
    {
        constexpr string_literal(const char (&str)[n])
        {
            std::copy_n(str, n, value);
        }

        char value[n];
    };

    template<mos::string_literal name>
    struct NamedType
    {
        // void *operator new(size_t size) = delete;
        // void *operator new(size_t size, void *ptr)
        // {
        //     return ptr;
        // }
        static constexpr mos::string_view type_name = name.value;
    };

    template<typename V, typename TSpecialisation = V>
    struct InitOnce
    {
        V *operator->()
        {
            return &value;
        }

        V &operator*()
        {
            return value;
        }

      private:
        static inline V value;
    };

#define PrivateTag                                                                                                                                                       \
  private:                                                                                                                                                               \
    struct Private                                                                                                                                                       \
    {                                                                                                                                                                    \
    };

} // namespace mos
