// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.hpp"

namespace mos
{
    template<typename T, typename... Args>
    requires HasTypeName<T> T *create(Args &&...args)
    {
        static InitOnce<Slab<T>> slab;
        return slab->create(args...);
    }
} // namespace mos
