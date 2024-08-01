// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syslog/syslog.h"

#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"
#include "proto/syslog.pb.h"

#include <mos/compiler.h>
#include <mos/mos_global.h>
#include <mos_stdio.h>
#include <pb_encode.h>

static spinlock_t global_syslog_lock = SPINLOCK_INIT;

long do_syslog(loglevel_t level, thread_t *thread, const char *file, const char *func, int line, debug_info_entry *feat, const char *fmt, ...)
{
    pb_syslog_message msg = {
        .timestamp = platform_get_timestamp(),
        .cpu_id = platform_current_cpu_id(),
    };

    msg.info.level = (syslog_level) level;
    msg.info.featid = feat ? feat->id : 0;
    msg.info.source_location.line = line;
    strncpy(msg.info.source_location.filename, file, sizeof(msg.info.source_location.filename));
    strncpy(msg.info.source_location.function, func, sizeof(msg.info.source_location.function));

    if (thread)
    {
        msg.thread.tid = thread->tid;
        msg.process.pid = thread->owner->pid;
        strncpy(msg.thread.name, thread->name, sizeof(msg.thread.name));
        strncpy(msg.process.name, thread->owner->name, sizeof(msg.process.name));
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.message, sizeof(msg.message), fmt, args);
    va_end(args);

    spinlock_acquire(&global_syslog_lock);

    lprintk(level, "\r\n");

#if MOS_CONFIG(MOS_PRINTK_WITH_TIMESTAMP)
    lprintk(MOS_LOG_UNSET, "%-16llu | ", platform_get_timestamp());
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_DATETIME)
    lprintk(MOS_LOG_UNSET, "%s | ", (const char *) platform_get_datetime_str());
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_CPU_ID)
    lprintk(MOS_LOG_UNSET, "cpu %2d | ", msg.cpu_id);
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_FILENAME)
    lprintk(MOS_LOG_UNSET, "%-15s | ", msg.info.source_location.filename);
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_THREAD_ID)
    lprintk(MOS_LOG_UNSET, "%pt\t| ", ((void *) thread));
#endif

    lprintk(MOS_LOG_UNSET, "%s", msg.message);

    spinlock_release(&global_syslog_lock);

    return 0;
}
