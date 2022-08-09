// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/device/console.h"

#include "lib/containers.h"
#include "mos/printk.h"

static int dummy_write(console_t *console, const char *message, size_t length)
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
    .write = dummy_write,
    .list_node = LIST_HEAD_INIT(consoles),
};

static bool has_console_registered = false;
list_node_t consoles = LIST_NODE_INIT(dummy_con);

void mos_register_console(console_t *con)
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
