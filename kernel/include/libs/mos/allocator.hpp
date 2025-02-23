// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.hpp"

namespace mos
{
    template<HasTypeName T, typename... Args>
    T *create(Args &&...args)
    {
        static InitOnce<Slab<T>> _slab;
        return _slab->create(args...);
    }
} // namespace mos
