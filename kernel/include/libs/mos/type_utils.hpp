// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <mos/mos_global.h>
#include <mos/string_view.hpp>

namespace mos
{
    template<auto n>
    struct string_literal
    {
        constexpr string_literal(const char (&str)[n])
        {
            std::copy_n(str, n, __data);
        }

        consteval char operator[](size_t i) const noexcept
        {
            return __data[i];
        }

        consteval const char *at(size_t i) const noexcept
        {
            return __data + i;
        }

        const size_t strlen = n;
        char __data[n];
    };

    struct _BaseNamedType
    {
    };

    template<mos::string_literal name>
    struct NamedType : public _BaseNamedType
    {
        static constexpr mos::string_view type_name = name.__data;
    };

    template<typename T>
    constexpr auto HasTypeName = std::is_base_of_v<_BaseNamedType, T>;

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

    template<typename T>
    consteval static inline string_view getTypeName()
    {
#if defined(MOS_COMPILER_CLANG) || defined(MOS_COMPILER_GCC)
        constexpr string_view f = __PRETTY_FUNCTION__;
        constexpr auto g = f.substr(f.find("with T = ") + 9);
        constexpr auto k = g.substr(0, g.find(";"));
        return k;
#else
#error "unknown compiler"
#endif
    }
} // namespace mos
