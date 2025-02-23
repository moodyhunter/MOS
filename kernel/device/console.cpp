// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syslog/syslog.hpp"
#include "mos/tasks/signal.hpp"
#include "mos/tasks/thread.hpp"

#include <array>
#include <limits.h>
#include <mos/device/console.hpp>
#include <mos/io/io.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/ring_buffer.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/tasks/wait.hpp>
#include <mos_string.hpp>

std::array<Console *, 128> consoles;
size_t console_list_size = 0;

void Console::putc(u8 c)
{
    if (c == 0x3)
    {
        spinlock_acquire(&waitlist.lock);
        list_foreach(waitable_list_entry_t, entry, waitlist.list)
        {
            Thread *thread = thread_get(entry->waiter);
            if (thread)
                signal_send_to_thread(thread, SIGINT);
        }
        spinlock_release(&waitlist.lock);
    }

    ring_buffer_pos_push_back_byte(reader.buf, &reader.pos, c);
    waitlist_wake(&waitlist, INT_MAX);
}

static size_t console_io_read(io_t *io, void *data, size_t size)
{
    Console *con = container_of(io, Console, io);

retry_read:;
    size_t read = 0;

    spinlock_acquire(&con->reader.lock);
    if (ring_buffer_pos_is_empty(&con->reader.pos))
    {
        spinlock_release(&con->reader.lock);
        bool ok = reschedule_for_waitlist(&con->waitlist);
        if (!ok)
        {
            pr_emerg("console: '%s' closed", con->name);
            return -EIO;
        }
        spinlock_acquire(&con->reader.lock);

        if (signal_has_pending())
        {
            spinlock_release(&con->reader.lock);
            return -ERESTARTSYS;
        }
    }
    else
    {
        read = ring_buffer_pos_pop_front(con->reader.buf, &con->reader.pos, (u8 *) data, size);
    }
    spinlock_release(&con->reader.lock);

    if (read == 0)
        goto retry_read;

    return read;
}

size_t console_io_write(io_t *io, const void *data, size_t size)
{
    Console *con = container_of(io, Console, io);
    spinlock_acquire(&con->writer.lock);
    if ((con->caps & CONSOLE_CAP_COLOR))
        con->set_color(con->default_fg, con->default_bg);
    size_t ret = con->do_write((const char *) data, size);
    spinlock_release(&con->writer.lock);
    return ret;
}

static const io_op_t console_io_ops = {
    .read = console_io_read,
    .write = console_io_write,
};

void console_register(Console *con)
{
    bool result = con->extra_setup();
    if (!result)
    {
        pr_emerg("console: failed to setup '%s'", con->name);
        return;
    }

    io_flags_t flags = IO_WRITABLE;

    if (con->caps & CONSOLE_CAP_READ)
    {
        MOS_ASSERT_X(con->reader.buf, "console: '%s' has no read buffer", con->name);
        ring_buffer_pos_init(&con->reader.pos, con->reader.size);
        flags |= IO_READABLE;
    }

    io_init(&con->io, IO_CONSOLE, flags, &console_io_ops);
    consoles[console_list_size++] = con;
    waitlist_init(&con->waitlist);

    if (printk_console == nullptr)
        printk_console = con;
}

Console *console_get(mos::string_view name)
{
    for (const auto &console : consoles)
    {
        if (console->name == name)
            return console;
    }

    return NULL;
}

Console *console_get_by_prefix(mos::string_view prefix)
{
    for (const auto &con : consoles)
    {
        if (con->name.starts_with(prefix))
            return con;
    }
    return NULL;
}
