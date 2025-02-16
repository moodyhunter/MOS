// SPDX-License-Identifier: GPL-3.0-or-later
#include <mos/mos_global.h>

#if MOS_CONFIG(MOS_PROFILING)
#include "mos/device/console.hpp"
#include "mos/misc/profiling.hpp"
#include "mos/misc/setup.hpp"
#include "mos/syslog/printk.hpp"

#include <mos/lib/structures/hashmap.hpp>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>
#endif

#if MOS_CONFIG(MOS_PROFILING)
#define PROFILER_HEADER "\nname,start_time,end_time,total_time\n"
#define PROFILER_LINE   "%s,%llu,%llu,%llu"

static console *profile_console = NULL;
static spinlock_t profile_lock static char profile_buffer[MOS_PRINTK_BUFFER_SIZE] = { 0 };
static char name_buffer[MOS_PRINTK_BUFFER_SIZE] = { 0 };

static bool profile_output_console(const char *console)
{
    profile_console = console_get(console);
    MOS_ASSERT(profile_console);
    console_write(profile_console, PROFILER_HEADER, strlen(PROFILER_HEADER));
    return true;
}

MOS_SETUP("profile_console", profile_output_console);

void profile_leave(const pf_point_t start, const char *fmt, ...)
{
    const u64 end = platform_get_timestamp();

    if (unlikely(!profile_console))
        return;

    const u64 total = end - start;

    spinlock_acquire(&profile_lock);
    va_list args;
    va_start(args, fmt);
    vsnprintf(name_buffer, sizeof(name_buffer), fmt, args);
    va_end(args);

    const size_t len = snprintf(profile_buffer, sizeof(profile_buffer), PROFILER_LINE "\n", name_buffer, start, end, total);
    console_write(profile_console, profile_buffer, len);
    spinlock_release(&profile_lock);
}

#endif
