// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/device/console.h>
#include <mos/io/io.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/ring_buffer.h>
#include <mos/printk.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/wait.h>
#include <string.h>

list_head consoles = LIST_HEAD_INIT(consoles);

static size_t console_read(io_t *io, void *data, size_t size)
{
    console_t *con = container_of(io, console_t, io);

    spinlock_acquire(&con->read.lock);
    if (ring_buffer_pos_is_empty(&con->read.pos))
    {
        spinlock_release(&con->read.lock);
        bool ok = reschedule_for_waitlist(&con->waitlist);
        if (!ok)
        {
            pr_emerg("console: '%s' closed", con->name);
            return 0;
        }
        spinlock_acquire(&con->read.lock);
    }

    const size_t rd = ring_buffer_pos_pop_front(con->read.buf, &con->read.pos, data, size);
    spinlock_release(&con->read.lock);

    return rd;
}

static size_t console_write(io_t *io, const void *data, size_t size)
{
    console_t *con = container_of(io, console_t, io);
    spinlock_acquire(&con->write.lock);
    size_t ret = con->ops->write(con, data, size);
    spinlock_release(&con->write.lock);
    return ret;
}

static const io_op_t console_io_ops = {
    .read = console_read,
    .write = console_write,
};

void console_register(console_t *con, size_t buf_size)
{
    if (con->caps & CONSOLE_CAP_EXTRA_SETUP)
    {
        bool result = con->ops->extra_setup(con);
        if (!result)
        {
            pr_emerg("console: failed to setup '%s'", con->name);
            return;
        }
    }

    MOS_ASSERT_X(con->read.buf, "console: '%s' has no read buffer", con->name);

    con->read.lock = (spinlock_t) SPINLOCK_INIT;
    con->write.lock = (spinlock_t) SPINLOCK_INIT;

    ring_buffer_pos_init(&con->read.pos, buf_size);
    io_init(&con->io, IO_CONSOLE, IO_READABLE | IO_WRITABLE, &console_io_ops);
    list_node_append(&consoles, list_node(con));
    waitlist_init(&con->waitlist);
    pr_info("console: registered '%s'", con->name);
}

console_t *console_get(const char *name)
{
    list_foreach(console_t, con, consoles)
    {
        if (strcmp(con->name, name) == 0)
            return con;
    }
    return NULL;
}

console_t *console_get_by_prefix(const char *prefix)
{
    list_foreach(console_t, con, consoles)
    {
        if (strncmp(con->name, prefix, strlen(prefix)) == 0)
            return con;
    }
    return NULL;
}

size_t console_write_color(console_t *con, const char *data, size_t size, standard_color_t fg, standard_color_t bg)
{
    standard_color_t prev_fg, prev_bg;
    spinlock_acquire(&con->write.lock);
    if (con->caps & CONSOLE_CAP_COLOR)
    {
        con->ops->get_color(con, &prev_fg, &prev_bg);
        con->ops->set_color(con, fg, bg);
    }

    size_t ret = con->ops->write(con, data, size);

    if (con->caps & CONSOLE_CAP_COLOR)
        con->ops->set_color(con, prev_fg, prev_bg);
    spinlock_release(&con->write.lock);
    return ret;
}

void console_putc(console_t *con, u8 c)
{
    ring_buffer_pos_push_back_byte(con->read.buf, &con->read.pos, c);
    waitlist_wake(&con->waitlist, INT_MAX);
}
