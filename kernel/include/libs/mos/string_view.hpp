// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

namespace mos
{
    template<typename CharT>
    constexpr auto generic_strlen(const CharT *c)
    {
        if (!c)
            return size_t(0);
        size_t len = 0;
        while (*(c++))
        {
            len++;
        }
        return len;
    }

    template<typename CharT>
    constexpr auto generic_strnlen(const CharT *c, size_t max)
    {
        size_t len = 0;
        while (len < max && *(c++))
        {
            len++;
        }
        return len;
    }

    template<typename CharT>
    constexpr auto generic_strncmp(const CharT *a, const CharT *b, size_t n)
    {
        for (size_t i = 0; i < n; i++)
        {
            if (a[i] != b[i])
                return a[i] - b[i];
        }
        return 0;
    }

    template<typename CharT>
    class basic_string_view
    {
      public:
        basic_string_view(std::nullptr_t) = delete;
        constexpr basic_string_view() : _pointer(nullptr), _length(0) {};
        constexpr basic_string_view(const CharT *cs) : _pointer(cs), _length(generic_strlen(cs)) {};
        constexpr basic_string_view(const CharT *s, size_t length) : _pointer(s), _length(length) {};

        const CharT *data() const
        {
            return _pointer;
        }

        const CharT &operator[](size_t index) const
        {
            return _pointer[index];
        }

        size_t size() const
        {
            return _length;
        }

        constexpr bool empty() const
        {
            return _length == 0;
        }

        bool operator==(basic_string_view other) const
        {
            if (_length != other._length)
                return false;

            return generic_strncmp(_pointer, other._pointer, _length) == 0;
        }

      private:
        const CharT *const _pointer;
        const size_t _length;
    };

    typedef basic_string_view<char> string_view;

    namespace string_literals
    {
        constexpr string_view operator"" _sv(const char *str, size_t len)
        {
            return string_view(str, len);
        }
    } // namespace string_literals
} // namespace mos
