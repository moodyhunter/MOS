// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/console.h"

#include "lib/string.h"
#include "lib/structures/list.h"
#include "mos/printk.h"

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

static bool has_console_registered = false;
list_node_t consoles = LIST_NODE_INIT(dummy_con);

void console_register(console_t *con)
{
    if (con->caps & CONSOLE_CAP_SETUP)
        con->setup(con);

    if (con->caps & CONSOLE_CAP_CLEAR)
        con->clear(con);

    list_node_append(&consoles, list_node(con));
    pr_info("console: registered '%s'", con->name);

    if (unlikely(!has_console_registered))
    {
        pr_info2("console: removing '%s'", dummy_con.name);
        list_remove(&dummy_con);
        has_console_registered = true;
    }
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
    return con->write_impl(con, data, size);
}
