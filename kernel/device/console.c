// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/device/console.h"

#include "mos/kernel.h"

console_t *platform_consoles = NULL;

void register_console(console_t *con)
{
    if (con->caps & CONSOLE_CAP_SETUP)
        con->setup(con);

    if (con->caps & CONSOLE_CAP_CLEAR)
        con->clear(con);

    pr_info("Registered console %s\n", con->name);

    if (platform_consoles == NULL)
    {
        platform_consoles = con;
        platform_consoles->__list_node.next = NULL;
        platform_consoles->__list_node.prev = NULL;
    }
    else
        list_append(platform_consoles, con);
}
