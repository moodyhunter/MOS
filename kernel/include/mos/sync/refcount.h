// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

typedef volatile size_t mos_refcount_t;

should_inline void refcount_inc(mos_refcount_t *atomic)
{
    __atomic_fetch_add(atomic, 1, __ATOMIC_RELAXED);
}

should_inline void refcount_dec(mos_refcount_t *atomic)
{
    __atomic_fetch_sub(atomic, 1, __ATOMIC_RELAXED);
}

should_inline void refcount_zero(mos_refcount_t *atomic)
{
    __atomic_store_n(atomic, 0, __ATOMIC_RELAXED);
}

should_inline size_t refcount_get(mos_refcount_t *atomic)
{
    return __atomic_load_n(atomic, __ATOMIC_RELAXED);
}
