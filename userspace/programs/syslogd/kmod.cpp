// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/io.hpp"
#include "mos/ipc/ipc_io.hpp"
#include "mos/kmod/kmod-decl.hpp"
#include "mos/misc/kutils.hpp"
#include "mos/platform/platform.hpp"
#include "mos/tasks/kthread.hpp"
#include "mos/tasks/process.hpp"
#include "proto/syslog.pb.h"
#include "syslogd.hpp"

#include <libipc/ipc.h>
#include <mos/filesystem/fs_types.h>
#include <pb_encode.h>

static IO *server = nullptr;

static long handle_log(void *arg, size_t argSize)
{
    if (argSize < sizeof(SyslogRequest))
        return -EINVAL;

    // Write the log message to the syslog pipe
    const auto logMessage = reinterpret_cast<SyslogRequest *>(arg);
    size_t messageSize = strlen(logMessage->message);
    if (messageSize == 0)
    {
        mWarn << "Empty log message, nothing to write";
        return -EINVAL; // Invalid message
    }
    timeval_t tv;
    platform_get_time(&tv);
    if (tv.day == 0)
        return -ENOTSUP;

    const auto days = days_from_civil(tv.year, tv.month, tv.day);
    const auto message = mos::string{ logMessage->message, logMessage->messageSize };

    pb_syslog_message val = {};
    val.message = const_cast<char *>(message.c_str());
    val.cpu_id = platform_current_cpu_id();
    val.thread.tid = current_thread->tid;
    val.thread.name = const_cast<char *>(current_thread->name.value_or("unknown").data());
    val.process.pid = current_process->pid;
    val.process.name = const_cast<char *>(current_process->name.value_or("unknown").data());
    val.timestamp = (days * 86400) + (tv.hour * 60 * 60) + (tv.minute * 60) + tv.second;
    val.info.level = static_cast<syslog_level>(logMessage->level);
    val.info.featid = 0; // Feature ID can be set to 0 or a specific value if needed

    size_t bufsize;
    pb_get_encoded_size(&bufsize, pb_syslog_message_fields, &val);
    pb_byte_t buffer[bufsize];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, bufsize);
    pb_encode(&stream, pb_syslog_message_fields, &val);

    if (!ipc_write_as_msg(server, buffer, bufsize))
    {
        mWarn << "Failed to write log message to syslog pipe";
        return -EIO; // Input/output error
    }
    return message.size(); // Return the number of bytes written
}

struct SyslogIO : public IO, public mos::NamedType<"module.syslog.io">
{
    SyslogIO() : IO(IO_WRITABLE, IO_IPC) {};

    void on_closed() override {};
    size_t on_write(const void *data, size_t size) override
    {
        if (!server)
        {
            mWarn << "Syslog server is not connected, cannot write log message";
            return -ENOTCONN; // Return error if server is not connected
        }

        SyslogRequest request;
        request.level = SyslogLevel::Info; // Default log level
        request.message = reinterpret_cast<const char *>(data);
        request.messageSize = size;
        return handle_log(&request, sizeof(request));
    }
};

static long open_syslogfd(void *, size_t argSize)
{
    if (argSize > 0)
        return -EINVAL;

    auto io = mos::create<SyslogIO>();
    if (!io)
    {
        mWarn << "Failed to allocate SyslogIO instance";
        return -ENOMEM; // Memory allocation failed
    }

    auto fd = process_attach_ref_fd(current_process, io, FD_FLAGS_NONE);
    if (fd < 0)
    {
        mWarn << "Failed to attach SyslogIO to process, error: " << fd;
        return fd; // Return the error code from attaching file descriptor
    }

    return fd; // Return the file descriptor to the caller
}

static long open_reader(void *arg, size_t argSize)
{
    if (argSize < sizeof(OpenReaderRequest))
        return -EINVAL;

    auto request = reinterpret_cast<OpenReaderRequest *>(arg);

    const auto io = ipc_connect(SYSLOGD_MODULE_NAME, 1024);
    if (io.isErr())
    {
        mWarn << "Failed to connect to syslog IPC server: " << io.getErr();
        return io.getErr(); // Return the error code from IPC connection
    }

    request->fd = process_attach_ref_fd(current_process, io.get(), FD_FLAGS_NONE);
    if (request->fd < 0)
    {
        mWarn << "Failed to attach file descriptor for syslog reader: " << request->fd;
        return request->fd; // Return the error code from attaching file descriptor
    }

    return 0;
}

static void syslogd_ipc_accepter(void *arg)
{
    IO *const controlIo = reinterpret_cast<IO *>(arg);
    const auto acc = ipc_accept(controlIo);
    if (acc.isErr())
    {
        mWarn << "Failed to accept connection on syslog IPC server: " << acc.getErr();
        return; // Exit if accepting connection failed
    }

    server = acc.get();
}

static void syslogd_kmod_entrypoint(ptr<mos::kmods::Module> self)
{
    self->ExportFunction("log", handle_log);
    self->ExportFunction("open_syslogfd", open_syslogfd);
    self->ExportFunction("open_reader", open_reader);

    const auto io = ipc_create(SYSLOGD_MODULE_NAME, 1);
    if (io.isErr())
    {
        mWarn << "Failed to create syslog IPC server: " << io.getErr();
        return; // Exit if IPC server creation failed
    }

    kthread_create(syslogd_ipc_accepter, io.get(), "syslogd-ipc-server");
}

KMOD_AUTHOR("MOS Developers");
KMOD_DESCRIPTION("Syslog Daemon Kernel Module");
KMOD_ENTRYPOINT(syslogd_kmod_entrypoint);
