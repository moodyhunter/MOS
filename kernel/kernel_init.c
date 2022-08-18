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

    cmdline_t *cmdline = mos_cmdline_parse(init_info->cmdline);
    mos_cmdline = cmdline;

    pr_info("kernel arguments: (total of %zu options)", cmdline->args_count);
    for (u32 i = 0; i < cmdline->args_count; i++)
    {
        cmdline_arg_t *option = cmdline->arguments[i];
        pr_info("%2d: %s", i, option->arg_name);

        for (u32 j = 0; j < option->param_count; j++)
        {
            cmdline_param_t *parameter = option->params[j];
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

    if (mos_cmdline_get_arg("mos_run_kernel_tests"))
    {
        mos_test_engine_run_tests();
    }
}
