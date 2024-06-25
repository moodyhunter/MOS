// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syslog/debug.h"

#include "mos/misc/setup.h"

#if MOS_CONFIG(MOS_DYNAMIC_DEBUG)
#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/syslog/printk.h"
#endif

#include <mos/mos_global.h>

#define _check_debug_macro_defined_(name) MOS_STATIC_ASSERT(MOS_CONCAT(MOS_DEBUG_, name) != 0);
MOS_ALL_DEBUG_MODULES(_check_debug_macro_defined_)
#undef _check_debug_macro_defined_

#if MOS_CONFIG(MOS_DYNAMIC_DEBUG)

// populate default debug settings
typeof(mos_debug_info) mos_debug_info = {
#define X(name) .name = MOS_DEBUG_FEATURE(name),
    MOS_ALL_DEBUG_MODULES(X)
#undef X
};

// expose a sysfs file to enable/disable each debug feature

void debug_print_action(const char *name, bool newstate)
{
    pr_info("debug option '%s' has been turned %s", name, newstate ? "on" : "off");
}

#define debug_show_function(name)                                                                                                                                        \
    bool debug_show_##name(sysfs_file_t *file)                                                                                                                           \
    {                                                                                                                                                                    \
        sysfs_printf(file, "%d\n", mos_debug_info.name);                                                                                                                 \
        return true;                                                                                                                                                     \
    }

#define debug_store_function(name)                                                                                                                                       \
    bool debug_store_##name(sysfs_file_t *file, const char *buf, size_t count, off_t offset)                                                                             \
    {                                                                                                                                                                    \
        MOS_UNUSED(file);                                                                                                                                                \
        MOS_UNUSED(offset);                                                                                                                                              \
        if (count < 1)                                                                                                                                                   \
            return false;                                                                                                                                                \
        const bool on = buf[0] == '1';                                                                                                                                   \
        mos_debug_info.name = on;                                                                                                                                        \
        debug_print_action(#name, on);                                                                                                                                   \
        return true;                                                                                                                                                     \
    }

#define X(name) debug_show_function(name) debug_store_function(name)
MOS_ALL_DEBUG_MODULES(X)
#undef X

static sysfs_item_t sys_debug_items[] = {
#define X(name) SYSFS_RW_ITEM(#name, debug_show_##name, debug_store_##name),
    MOS_ALL_DEBUG_MODULES(X)
#undef X
};

SYSFS_AUTOREGISTER(debug, sys_debug_items);

#define SETUP_DEBUG_MODULE(name)                                                                                                                                         \
    static bool setup_debug_##name(const char *value)                                                                                                                    \
    {                                                                                                                                                                    \
        mos_debug_info.name = cmdline_string_truthiness(value, true);                                                                                                    \
        return true;                                                                                                                                                     \
    }                                                                                                                                                                    \
    MOS_SETUP("debug." #name, setup_debug_##name);

MOS_ALL_DEBUG_MODULES(SETUP_DEBUG_MODULE)

#endif
