// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/setup.h"

#define MOS_SYSFS_AUTOREGISTER(sysfs_name, sysfs_items)                                                                                                                  \
    static sysfs_dir_t __sysfs_##sysfs_name = {                                                                                                                          \
        .list_node = LIST_NODE_INIT(__sysfs_##sysfs_name),                                                                                                               \
        .name = #sysfs_name,                                                                                                                                             \
        .items = sysfs_items,                                                                                                                                            \
    };                                                                                                                                                                   \
    static void __sysfs_##sysfs_name##_init(void)                                                                                                                        \
    {                                                                                                                                                                    \
        sysfs_register(&__sysfs_##sysfs_name);                                                                                                                           \
    }                                                                                                                                                                    \
    MOS_INIT(SYSFS, __sysfs_##sysfs_name##_init)
