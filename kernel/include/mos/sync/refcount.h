// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

#include <stdatomic.h>

typedef volatile atomic_size_t mos_refcount_t;

should_inline void refcount_inc(mos_refcount_t *atomic)
{
    atomic_fetch_add_explicit(atomic, 1, memory_order_relaxed);
}

should_inline void refcount_dec(mos_refcount_t *atomic)
{
    atomic_fetch_sub_explicit(atomic, 1, memory_order_relaxed);
}

should_inline void refcount_zero(mos_refcount_t *atomic)
{
    atomic_store_explicit(atomic, 0, memory_order_relaxed);
}

should_inline size_t refcount_get(mos_refcount_t *atomic)
{
    return atomic_load_explicit(atomic, memory_order_relaxed);
}
