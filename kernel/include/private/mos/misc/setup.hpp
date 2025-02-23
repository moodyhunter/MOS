// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/string_view.hpp>
#include <mos/types.hpp>

typedef enum
{
    INIT_TARGET_POWER,   // Power management subsystem
    INIT_TARGET_PRE_VFS, // Pre-virtual file system
    INIT_TARGET_VFS,     // Virtual file system
    INIT_TARGET_SYSFS,   // sysfs filesystem
    INIT_TARGET_KTHREAD, // Kernel threads
} init_target_t;

typedef struct
{
    const char *param;
    bool (*hook)(mos::string_view arg);
} mos_cmdline_hook_t;

typedef struct
{
    init_target_t target;
    void (*init_fn)(void);
} mos_init_t;

#define MOS_EARLY_SETUP(_param, _fn)                                                                                                                                     \
    static bool _fn(mos::string_view arg);                                                                                                                               \
    MOS_PUT_IN_SECTION(".mos.early_setup", mos_cmdline_hook_t, __setup_##_fn, { .param = _param, .hook = _fn });                                                         \
    static bool _fn(mos::string_view arg)

#define MOS_SETUP(_param, _fn)                                                                                                                                           \
    static bool _fn(mos::string_view arg);                                                                                                                               \
    MOS_PUT_IN_SECTION(".mos.setup", mos_cmdline_hook_t, __setup_##_fn, { .param = _param, .hook = _fn });                                                               \
    static bool _fn(mos::string_view arg)

#define MOS_INIT(_comp, _fn)                                                                                                                                             \
    static void _fn(void);                                                                                                                                               \
    MOS_PUT_IN_SECTION(".mos.init", mos_init_t, __init_##_fn, { .target = INIT_TARGET_##_comp, .init_fn = _fn });                                                        \
    static void _fn(void)

void startup_invoke_cmdline_hooks(void);
void startup_invoke_early_cmdline_hooks(void);
void startup_invoke_autoinit(init_target_t target);
