// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>

template<class P, class M>
[[gnu::always_inline]] constexpr size_t __offsetof(const M P::*member)
{
    return (size_t) &(reinterpret_cast<P *>(0)->*member);
}

template<class P, class M>
[[gnu::always_inline]] constexpr inline P *__container_of(M *ptr, const M P::*member)
{
    return (P *) ((char *) ptr - __offsetof(member));
}

template<class P, class M>
[[gnu::always_inline]] constexpr inline const P *__container_of(const M *ptr, const M P::*member)
{
    return (const P *) ((char *) ptr - __offsetof(member));
}

#define container_of(ptr, type, member) __container_of(ptr, &type::member)
