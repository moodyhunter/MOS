// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.hpp"

#include <mos/type_utils.hpp>

namespace mos
{
    template<typename T, typename... Args>
    requires mos::HasTypeName<T> T *create(Args &&...args)
    {
        static InitOnce<Slab<T>> _slab;
        return _slab->create(args...);
    }
} // namespace mos
