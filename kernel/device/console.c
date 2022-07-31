// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/device/console.h"

#include "lib/containers.h"
#include "mos/kernel.h"
#include "mos/mos_global.h"

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
    .list_node = MOS_LIST_HEAD_INIT(consoles),
};
list_node_t consoles = MOS_LIST_NODE_INIT(dummy_con);

void register_console(console_t *con)
{
    if (con->caps & CONSOLE_CAP_SETUP)
        con->setup(con);

    if (con->caps & CONSOLE_CAP_CLEAR)
        con->clear(con);

    list_node_append(&consoles, list_node(con));
    pr_info("Registered console: '%s'", con->name);

    static bool has_console_registered = false;
    if (unlikely(!has_console_registered))
    {
        pr_debug("Removing dummy console: '%s'", dummy_con.name);
        list_remove(&dummy_con);
    }
}
