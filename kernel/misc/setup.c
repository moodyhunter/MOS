// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/setup.h"

#include "mos/misc/cmdline.h"
#include "mos/syslog/printk.h"

void startup_invoke_autoinit(init_target_t target)
{
    extern const mos_init_t __MOS_INIT_START[];
    extern const mos_init_t __MOS_INIT_END[];

    for (const mos_init_t *init = __MOS_INIT_START; init < __MOS_INIT_END; init++)
    {
        if (init->target == target)
            init->init_fn();
    }
}

static void do_invoke_setup(const mos_cmdline_hook_t start[], const mos_cmdline_hook_t end[])
{
    for (const mos_cmdline_hook_t *func = start; func < end; func++)
    {
        cmdline_option_t *option = cmdline_get_option(func->param);

        if (unlikely(!option))
        {
            pr_dinfo2(setup, "no option given for '%s'", func->param);
            continue;
        }

        if (unlikely(option->used))
        {
            pr_warn("option '%s' already used", func->param);
            continue;
        }

        pr_dinfo2(setup, "invoking setup function for '%s'", func->param);
        if (unlikely(!func->hook(option->arg)))
        {
            pr_warn("setup function for '%s' failed", func->param);
            continue;
        }

        option->used = true;
    }
}

void startup_invoke_cmdline_hooks(void)
{
    extern const mos_cmdline_hook_t __MOS_SETUP_START[]; // defined in linker script
    extern const mos_cmdline_hook_t __MOS_SETUP_END[];
    do_invoke_setup(__MOS_SETUP_START, __MOS_SETUP_END);
}

void startup_invoke_early_cmdline_hooks(void)
{
    extern const mos_cmdline_hook_t __MOS_EARLY_SETUP_START[]; // defined in linker script
    extern const mos_cmdline_hook_t __MOS_EARLY_SETUP_END[];
    do_invoke_setup(__MOS_EARLY_SETUP_START, __MOS_EARLY_SETUP_END);
}
