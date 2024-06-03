// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/misc/setup.h"

#define _DO_SLAB_AUTOINIT(name, var, type, func)                                                                                                                         \
    static void func(void)                                                                                                                                               \
    {                                                                                                                                                                    \
        var = kmemcache_create(name, sizeof(type));                                                                                                                      \
    }                                                                                                                                                                    \
    MOS_INIT(SLAB_AUTOINIT, func)

#define SLAB_AUTOINIT(name, var, type) _DO_SLAB_AUTOINIT(name, var, type, MOS_CONCAT(__slab_autoinit_, __COUNTER__))
