// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/signal.hpp"
#include "mos/tasks/thread.hpp"

#include <limits.h>
#include <mos/device/console.hpp>
#include <mos/io/io.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/ring_buffer.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/tasks/wait.hpp>
#include <mos_string.hpp>

list_head consoles = LIST_HEAD_INIT(consoles);

void Console::putc(u8 c)
{
    if (c == 0x3)
    {
        spinlock_acquire(&waitlist.lock);
        list_foreach(waitable_list_entry_t, entry, waitlist.list)
        {
            thread_t *thread = thread_get(entry->waiter);
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

static size_t console_io_write(io_t *io, const void *data, size_t size)
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

    MOS_ASSERT_X(con->name != NULL, "console: %p's name is NULL", con);

    con->writer.lock = (spinlock_t) SPINLOCK_INIT;
    io_flags_t flags = IO_WRITABLE;

    if (con->caps & CONSOLE_CAP_READ)
    {
        MOS_ASSERT_X(con->reader.buf, "console: '%s' has no read buffer", con->name);
        con->reader.lock = SPINLOCK_INIT;
        ring_buffer_pos_init(&con->reader.pos, con->reader.size);
        flags |= IO_READABLE;
    }

    io_init(&con->io, IO_CONSOLE, flags, &console_io_ops);
    list_node_append(&consoles, list_node(con));
    waitlist_init(&con->waitlist);
}

Console *console_get(const char *name)
{
    if (list_is_empty(&consoles))
        return NULL;

    list_foreach(Console, con, consoles)
    {
        if (strcmp(con->name, name) == 0)
            return con;
    }
    return NULL;
}

Console *console_get_by_prefix(const char *prefix)
{
    list_foreach(Console, con, consoles)
    {
        if (strncmp(con->name, prefix, strlen(prefix)) == 0)
            return con;
    }
    return NULL;
}
