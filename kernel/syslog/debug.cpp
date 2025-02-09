// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syslog/debug.hpp"

#include "mos/misc/setup.hpp"

#if MOS_CONFIG(MOS_DYNAMIC_DEBUG)
#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/syslog/printk.hpp"
#endif

#include <mos/mos_global.h>

#define _check_debug_macro_defined_(name) MOS_STATIC_ASSERT(MOS_CONCAT(MOS_DEBUG_, name) != 0);
MOS_ALL_DEBUG_MODULES(_check_debug_macro_defined_)
#undef _check_debug_macro_defined_

#if MOS_CONFIG(MOS_DYNAMIC_DEBUG)

// populate default debug settings
struct _mos_debug_info mos_debug_info = {
#define X(_name) ._name = { .id = __COUNTER__ + 1, .name = #_name, .enabled = MOS_DEBUG_FEATURE(_name) },
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
        sysfs_printf(file, "%d\n", mos_debug_info.name.enabled);                                                                                                         \
        return true;                                                                                                                                                     \
    }

#define debug_store_function(name)                                                                                                                                       \
    size_t debug_store_##name(sysfs_file_t *file, const char *buf, size_t count, off_t offset)                                                                           \
    {                                                                                                                                                                    \
        MOS_UNUSED(file);                                                                                                                                                \
        MOS_UNUSED(offset);                                                                                                                                              \
        if (count < 1)                                                                                                                                                   \
            return -EINVAL;                                                                                                                                              \
        const bool on = buf[0] == '1';                                                                                                                                   \
        mos_debug_info.name.enabled = on;                                                                                                                                \
        debug_print_action(#name, on);                                                                                                                                   \
        return count;                                                                                                                                                    \
    }

MOS_ALL_DEBUG_MODULES(debug_show_function)
MOS_ALL_DEBUG_MODULES(debug_store_function)

static sysfs_item_t sys_debug_items[] = {
#define X(name) SYSFS_RW_ITEM(#name, debug_show_##name, debug_store_##name),
    MOS_ALL_DEBUG_MODULES(X)
#undef X
};

SYSFS_AUTOREGISTER(debug, sys_debug_items);

#define SETUP_DEBUG_MODULE(name)                                                                                                                                         \
    static bool setup_debug_##name(const char *value)                                                                                                                    \
    {                                                                                                                                                                    \
        mos_debug_info.name.enabled = cmdline_string_truthiness(value, true);                                                                                            \
        return true;                                                                                                                                                     \
    }                                                                                                                                                                    \
    MOS_SETUP("debug." #name, setup_debug_##name);

MOS_ALL_DEBUG_MODULES(SETUP_DEBUG_MODULE)

// ! expose debug info id to userspace
#define debug_show_id_function(name)                                                                                                                                     \
    bool debug_show_id_##name(sysfs_file_t *file)                                                                                                                        \
    {                                                                                                                                                                    \
        sysfs_printf(file, "%d\n", mos_debug_info.name.id);                                                                                                              \
        return true;                                                                                                                                                     \
    }

MOS_ALL_DEBUG_MODULES(debug_show_id_function)

static sysfs_item_t sys_debug_id_items[] = {
#define X(name) SYSFS_RO_ITEM(#name, debug_show_id_##name),
    MOS_ALL_DEBUG_MODULES(X)
};

SYSFS_AUTOREGISTER(debug_id, sys_debug_id_items);

#endif
