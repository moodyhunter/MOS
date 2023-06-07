// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.h"
#include "mos/setup.h"

#define MOS_SLAB_AUTOINIT(name, var, type)                                                                                                                               \
    static void __slab_autoinit_##var(void)                                                                                                                              \
    {                                                                                                                                                                    \
        var = kmemcache_create(name, sizeof(type));                                                                                                                      \
    }                                                                                                                                                                    \
    MOS_INIT(SLAB_AUTOINIT, __slab_autoinit_##var)
