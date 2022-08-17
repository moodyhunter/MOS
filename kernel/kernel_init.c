// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"
#include "mos/mm/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

extern void mos_test_engine_run_tests(); // defined in tests/test_engine.c

void mos_start_kernel(mos_init_info_t *init_info)
{
    mos_init_kernel_mm();
    mos_platform.post_init(init_info);
    mos_platform.devices_setup(init_info);
    mos_platform.interrupt_enable();

    cmdline_t *cmdline = parse_cmdline(init_info->cmdline);

    pr_info("kernel arguments: (total of %d options)", cmdline->options_count);
    for (u32 i = 0; i < cmdline->options_count; i++)
    {
        cmdline_option_t *option = cmdline->options[i];
        pr_info("%2d: %s", i, option->name);

        for (u32 j = 0; j < option->parameters_count; j++)
        {
            cmdline_parameter_t *parameter = option->parameters[j];
            switch (parameter->param_type)
            {
                case CMDLINE_PARAM_TYPE_STRING: pr_info("%6s%s", "", parameter->val.string); break;
                case CMDLINE_PARAM_TYPE_BOOL: pr_info("%6s%s", "", parameter->val.boolean ? "true" : "false"); break;
            }
        }
    }

    pr_info("Welcome to MOS!");
    pr_info("Boot Information:");
    pr_emph("cmdline: %s", init_info->cmdline);
    pr_emph("%-25s'%s'", "Kernel Version:", MOS_KERNEL_VERSION);
    pr_emph("%-25s'%s'", "Kernel Revision:", MOS_KERNEL_REVISION);
    pr_emph("%-25s'%s'", "Kernel builtin cmdline:", MOS_KERNEL_BUILTIN_CMDLINE);

    mos_warn("V2Ray 4.45.2 started");

    if (cmdline_get_option(cmdline, "mos_run_kernel_tests"))
    {
        mos_test_engine_run_tests();
    }
}
