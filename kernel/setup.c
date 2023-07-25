// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/setup.h"

#include "mos/cmdline.h"
#include "mos/printk.h"

void setup_reach_init_target(init_target_t target)
{
    extern const mos_init_t __MOS_INIT_START[];
    extern const mos_init_t __MOS_INIT_END[];

    for (const mos_init_t *init = __MOS_INIT_START; init < __MOS_INIT_END; init++)
    {
        if (init->target == target)
            init->init_fn();
    }
}

void setup_invoke_setup(void)
{
    extern const mos_setup_t __MOS_SETUP_START[]; // defined in linker script
    extern const mos_setup_t __MOS_SETUP_END[];

    for (const mos_setup_t *func = __MOS_SETUP_START; func < __MOS_SETUP_END; func++)
    {
        cmdline_option_t *option = cmdline_get_option(func->param);

        if (unlikely(!option))
        {
            mos_debug(setup, "no option given for '%s'", func->param);
            continue;
        }

        if (unlikely(option->used))
        {
            pr_warn("option '%s' already used", func->param);
            continue;
        }

        mos_debug(setup, "invoking setup function for '%s'", func->param);
        if (unlikely(!func->setup_fn(option->arg)))
        {
            pr_warn("setup function for '%s' failed", func->param);
            continue;
        }

        option->used = true;
    }
}

void setup_invoke_earlysetup(void)
{
    extern const mos_setup_t __MOS_EARLY_SETUP_START[]; // defined in linker script
    extern const mos_setup_t __MOS_EARLY_SETUP_END[];

    for (const mos_setup_t *func = __MOS_EARLY_SETUP_START; func < __MOS_EARLY_SETUP_END; func++)
    {
        cmdline_option_t *option = cmdline_get_option(func->param);

        if (unlikely(!option))
        {
            mos_debug(setup, "no option given for '%s'", func->param);
            continue;
        }

        mos_debug(setup, "invoking early setup function for '%s'", func->param);
        if (unlikely(!func->setup_fn(option->arg)))
            pr_warn("early setup function for '%s' failed", func->param);

        option->used = true;
    }
}
