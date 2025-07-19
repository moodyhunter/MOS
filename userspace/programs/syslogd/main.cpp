// SPDX-License-Identifier: GPL-3.0-or-later

#include "libsm.h"
#include "proto/syslog.pb.h"
#include "syslogd.hpp"

#include <chrono>
#include <iostream>
#include <libipc/ipc.h>
#include <librpc/internal.h>
#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <mos/syscall/usermode.h>
#include <pb.h>
#include <pb_decode.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

constexpr auto SYSLOG_MODULE_PATH = "/initrd/modules/syslogd.ko";

void doReadOnFd(fd_t fd)
{
    while (true)
    {
        ipc_msg_t *msg = ipc_read_msg(fd);
        if (!msg)
        {
            puts("EOF reached on syslog reader, exiting...");
            break;
        }

        if (msg->size == 0)
        {
            puts("Received empty message, skipping...");
            ipc_msg_destroy(msg);
            continue;
        }

        pb_syslog_message val = {};
        pb_istream_t stream = pb_istream_from_buffer(reinterpret_cast<pb_byte_t *>(msg->data), msg->size);
        if (!pb_decode(&stream, pb_syslog_message_fields, &val))
        {
            puts("Failed to decode syslog message");
            ipc_msg_destroy(msg);
            continue; // Skip messages that cannot be decoded
        }

        std::string_view msg_view(val.message, val.message ? strlen(val.message) : 0);
        if (msg_view.ends_with('\n'))
            msg_view.remove_suffix(1); // Remove trailing newline if present

        const auto timestamp = std::chrono::system_clock::time_point(std::chrono::seconds(val.timestamp));
        std::time_t time_t = std::chrono::system_clock::to_time_t(timestamp);
        std::tm *tm = std::localtime(&time_t);
        char time_buffer[64];
        std::strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", tm);

        std::string_view process_name(val.process.name ? val.process.name : "unknown");
        std::string_view thread_name(val.thread.name ? val.thread.name : "unknown");
        std::string_view level_str;
        switch (val.info.level)
        {
            case syslog_level::syslog_level_UNSET: level_str = "UNSET"; break;
            case syslog_level::syslog_level_INFO2: level_str = "INFO2"; break;
            case syslog_level::syslog_level_INFO: level_str = "INFO "; break;
            case syslog_level::syslog_level_EMPH: level_str = "EMPH "; break;
            case syslog_level::syslog_level_WARN: level_str = "WARN "; break;
            case syslog_level::syslog_level_EMERG: level_str = "EMERG"; break;
            case syslog_level::syslog_level_FATAL: level_str = "FATAL"; break;
            default: level_str = "UNKNOWN"; break;
        }

        std::cout << "[" << time_buffer << "] "                            //
                  << "CPU: " << val.cpu_id                                 //
                  << "[" << val.process.pid << ":" << process_name << "] " //
                  << "[" << val.thread.tid << ":" << thread_name << "] "   //
                  << level_str                                             //
                  << ": " << msg_view << std::endl;

        ipc_msg_destroy(msg);                       // Clean up the message after processing
        pb_release(pb_syslog_message_fields, &val); // Release the decoded message
    }
    close(fd);
}

int main(int, char **)
{
    if (syscall_kmod_load(SYSLOG_MODULE_PATH))
    {
        puts("Failed to load syslogd kernel module");
        ReportServiceState(UnitStatus::Failed, "syslogd kernel module load failed");
        return -1;
    }

    OpenReaderRequest request;
    if (syscall_kmod_call(SYSLOGD_MODULE_NAME, "open_reader", &request, sizeof(request)) != 0)
    {
        puts("Failed to open syslog reader");
        ReportServiceState(UnitStatus::Failed, "syslogd reader open failed");
        return -1;
    }

    if (request.fd < 0)
    {
        puts("Failed to open syslog reader");
        ReportServiceState(UnitStatus::Failed, "syslogd reader open failed");
        return -1;
    }

    ReportServiceState(UnitStatus::Started, "syslogd started");
    doReadOnFd(request.fd);
    return 0;
}
