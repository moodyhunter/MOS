// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/device/console.h>
#include <mos/lib/structures/list.h>
#include <mos/printk.h>
#include <string.h>

static int dummy_write_to_console(console_t *console, const char *message, size_t length)
{
    (void) console;
    (void) message;
    (void) length;
    return 0;
}

// ! This is a compile-time connected linked list.
static console_t dummy_con = {
    .name = "dummy",
    .caps = CONSOLE_CAP_NONE,
    .write_impl = dummy_write_to_console,
    .list_node = LIST_HEAD_INIT(consoles),
};

list_head consoles = LIST_NODE_INIT(dummy_con);

void console_register(console_t *con)
{
    if (con->caps & CONSOLE_CAP_SETUP)
        con->setup(con);

    if (con->caps & CONSOLE_CAP_CLEAR)
        con->clear(con);

    con->lock = (spinlock_t) SPINLOCK_INIT;

    list_node_append(&consoles, list_node(con));
    pr_info("console: registered '%s'", con->name);

    if (unlikely(once())) // remove the dummy console
        list_remove(&dummy_con);
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

int console_read(console_t *con, char *dest, size_t size)
{
    if (con->caps & CONSOLE_CAP_READ)
        return con->read_impl(con, dest, size);
    return -1;
}

int console_write(console_t *con, const char *data, size_t size)
{
    spinlock_acquire(&con->lock);
    int sz = con->write_impl(con, data, size);
    spinlock_release(&con->lock);
    return sz;
}

int console_write_color(console_t *con, const char *data, size_t size, standard_color_t fg, standard_color_t bg)
{
    standard_color_t prev_fg, prev_bg;
    spinlock_acquire(&con->lock);
    if (con->caps & CONSOLE_CAP_COLOR)
    {
        con->get_color(con, &prev_fg, &prev_bg);
        con->set_color(con, fg, bg);
    }

    int ret = con->write_impl(con, data, size);

    if (con->caps & CONSOLE_CAP_COLOR)
        con->set_color(con, prev_fg, prev_bg);
    spinlock_release(&con->lock);
    return ret;
}
