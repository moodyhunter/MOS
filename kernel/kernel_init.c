// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"
#include "mos/mm/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

extern void mos_test_engine_run_tests(); // defined in tests/test_engine.c

void mos_start_kernel(mos_init_info_t *init_info)
{
    mos_kernel_mm_init();
    mos_platform.post_init(init_info);
    mos_platform.devices_setup(init_info);
    mos_platform.interrupt_enable();

    mos_cmdline = mos_cmdline_create(init_info->cmdline_str);

    pr_info("Welcome to MOS!");
    pr_emph("MOS %s (%s)", MOS_KERNEL_VERSION, MOS_KERNEL_REVISION);
    pr_emph("MOS Arguments: (total of %zu options)", mos_cmdline->args_count);
    for (u32 i = 0; i < mos_cmdline->args_count; i++)
    {
        cmdline_arg_t *option = mos_cmdline->arguments[i];
        pr_info("%2d: %s", i, option->arg_name);

        for (u32 j = 0; j < option->param_count; j++)
        {
            cmdline_param_t *parameter = option->params[j];
            switch (parameter->param_type)
            {
                case CMDLINE_PARAM_TYPE_STRING: pr_info("%6s%s", "", parameter->val.string); break;
                case CMDLINE_PARAM_TYPE_BOOL: pr_info("%6s%s", "", parameter->val.boolean ? "true" : "false"); break;
                default: MOS_UNREACHABLE();
            }
        }
    }
    pr_emph("%-25s'%s'", "Kernel builtin cmdline:", MOS_KERNEL_BUILTIN_CMDLINE);

    mos_warn("V2Ray 4.45.2 started");

    if (mos_cmdline_get_arg("mos_tests"))
    {
        mos_test_engine_run_tests();
    }
}
