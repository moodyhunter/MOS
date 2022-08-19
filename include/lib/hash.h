// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

always_inline hash_t __pure string_hash(const char *s, const int n)
{
    const int p = 31, m = 1e9 + 7;
    hash_t h = { 0 };
    long p_pow = 1;
    for (int i = 0; i < n; i++)
    {
        h.hash = (h.hash + (s[i] - 'a' + 1) * p_pow) % m;
        p_pow = (p_pow * p) % m;
    }
    return h;
}
