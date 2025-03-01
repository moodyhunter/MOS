// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syslog/syslog.hpp"

#include "mos/device/console.hpp"
#include "mos/platform/platform.hpp"
#include "mos/tasks/task_types.hpp"
#include "proto/syslog.pb.h"

#include <ansi_colors.h>
#include <mos/compiler.h>
#include <mos/mos_global.h>
#include <mos_stdio.hpp>
#include <pb_encode.h>

static spinlock_t global_syslog_lock;

static void do_print_syslog(const pb_syslog_message *msg, const debug_info_entry *feat)
{
    const LogLevel level = (LogLevel) msg->info.level;
    spinlock_acquire(&global_syslog_lock);

    if (level != LogLevel::UNSET)
    {
        lprintk(level, "\r\n");
        if (feat)
            lprintk(level, "%-10s | ", feat->name);

#if MOS_CONFIG(MOS_PRINTK_WITH_TIMESTAMP)
        lprintk(level, "%-16lu | ", msg->timestamp);
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_DATETIME)
        lprintk(level, "%s | ", (const char *) platform_get_datetime_str());
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_CPU_ID)
        lprintk(level, "cpu %2d | ", msg->cpu_id);
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_FILENAME)
        lprintk(level, "%-15s | ", msg->info.source_location.filename);
#endif

#if MOS_CONFIG(MOS_PRINTK_WITH_THREAD_ID)
        lprintk(level, "[t%d:%s]\t| ", msg->thread.tid, msg->thread.name);
#endif
    }

    lprintk(level, "%s", msg->message);

    spinlock_release(&global_syslog_lock);
}

long do_syslog(LogLevel level, const char *file, const char *func, int line, const debug_info_entry *feat, const char *fmt, ...)
{
    auto const thread = current_thread;
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
        strncpy(msg.thread.name, thread->name.c_str(), sizeof(msg.thread.name));
        strncpy(msg.process.name, thread->owner->name.c_str(), sizeof(msg.process.name));
    }

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg.message, sizeof(msg.message), fmt, args);
    va_end(args);

    do_print_syslog(&msg, feat);
    return 0;
}

mos::SyslogStreamWriter::SyslogStreamWriter(DebugFeature feature, LogLevel level, RCCore *rcCore, SyslogBuffer &fmtbuffer)
    : RefCounted(rcCore),                                                                //
      fmtbuffer(fmtbuffer),                                                              //
      timestamp(platform_get_timestamp()),                                               //
      feature(feature),                                                                  //
      level(level),                                                                      //
      should_print(!mos_debug_info_map[feature] || mos_debug_info_map[feature]->enabled) //
{
    if (level != LogLevel::UNSET)
    {
        pos++;
        fmtbuffer[0] = '\n';
        fmtbuffer[1] = '\0';
    }

    if (should_print && mos_debug_info_map[feature])
        pos += snprintf(fmtbuffer.data() + pos, MOS_PRINTK_BUFFER_SIZE - pos, "%-10s | ", mos_debug_info_map[feature]->name);
}

mos::SyslogStreamWriter::~SyslogStreamWriter()
{
    if (GetRef() == 1)
    {
        if (!should_print)
            return;

        if (unlikely(!printk_console))
            printk_console = consoles.front();

        print_to_console(printk_console, level, fmtbuffer.data(), pos);
        fmtbuffer[0] = '\0';
        pos = -1;
    }
}
