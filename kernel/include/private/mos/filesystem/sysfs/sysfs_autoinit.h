// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/setup.h"

#define SYSFS_AUTOREGISTER(sysfs_name, sysfs_items)                                                                                                                      \
    SYSFS_DEFINE_DIR(sysfs_name, sysfs_items);                                                                                                                           \
    static void __sysfs_##sysfs_name##_init(void)                                                                                                                        \
    {                                                                                                                                                                    \
        sysfs_register(&__sysfs_##sysfs_name);                                                                                                                           \
    }                                                                                                                                                                    \
    MOS_INIT(SYSFS, __sysfs_##sysfs_name##_init)
