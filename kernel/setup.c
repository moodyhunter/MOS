// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/setup.h"

#include "lib/string.h"
#include "mos/printk.h"

static cmdline_option_t *cmdline_get_option(const char *option_name)
{
    MOS_ASSERT(mos_cmdline);
    for (u32 i = 0; i < mos_cmdline->options_count; i++)
    {
        if (strcmp(mos_cmdline->options[i]->name, option_name) == 0)
            return mos_cmdline->options[i];
    }
    return NULL;
}

void invoke_setup_functions(cmdline_t *cmdline)
{
    extern const setup_func_t __MOS_SETUP_START[]; // defined in linker script
    extern const setup_func_t __MOS_SETUP_END[];

    for (const setup_func_t *func = __MOS_SETUP_START; func < __MOS_SETUP_END; func++)
    {
        cmdline_option_t *option = cmdline_get_option(func->param);
        if (unlikely(!option))
        {
            mos_debug(init, "no option given for '%s'", func->param);
            continue;
        }

        mos_debug(init, "invoking setup function for '%s'", func->param);
        const bool result = func->setup_fn(option->argc, option->argv);
        if (unlikely(!result))
        {
            pr_warn("setup function for '%s' failed", func->param);
            continue;
        }

        cmdline_remove_option(cmdline, option->name);
    }
}
