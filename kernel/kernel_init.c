// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"
#include "mos/device/block.h"
#include "mos/elf/elf.h"
#include "mos/filesystem/cpio/cpio.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/pathutils.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"

extern void mos_test_engine_run_tests(); // defined in tests/test_engine.c

bool mount_initrd(void)
{
    blockdev_t *dev = blockdev_find("initrd");
    if (!dev)
        return false;
    pr_info("found initrd block device: %s", dev->name);

    mountpoint_t *mount = kmount(&root_path, &fs_cpio, dev);
    if (!mount)
        return false;

    pr_info("mounted initrd as rootfs");
    return true;
}

const char *cmdline_get_init_path(void)
{
    cmdline_arg_t *init_arg = mos_cmdline_get_arg("init");
    if (init_arg && init_arg->params_count > 0)
    {
        cmdline_param_t *init_param = init_arg->params[0];
        if (init_param->param_type == CMDLINE_PARAM_TYPE_STRING)
            return init_param->val.string;

        pr_warn("init path is not a string, using default '/programs/init'");
    }
    return "/programs/init";
}

void dump_cmdline(void)
{
    pr_emph("MOS %s (%s)", MOS_KERNEL_VERSION, MOS_KERNEL_REVISION);
    pr_emph("MOS Arguments: (total of %zu options)", mos_cmdline->args_count);
    for (u32 i = 0; i < mos_cmdline->args_count; i++)
    {
        cmdline_arg_t *option = mos_cmdline->arguments[i];
        pr_info("%2d: %s", i, option->arg_name);

        for (u32 j = 0; j < option->params_count; j++)
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
}

void mos_start_kernel(const char *cmdline)
{
    mos_cmdline = mos_cmdline_create(cmdline);

    pr_info("Welcome to MOS!");
    dump_cmdline();

#if MOS_MEME
    mos_warn("V2Ray 4.45.2 started");
#endif

#if BUILD_TESTING
    if (mos_cmdline_get_arg("mos_tests"))
        mos_test_engine_run_tests();
#endif

    process_init();
    thread_init();

    if (unlikely(!mount_initrd()))
        mos_panic("failed to mount initrd");

    const char *init_path = cmdline_get_init_path();

    uid_t root_uid = 0;
    process_t *init = process_create_from_elf(init_path, NULL, root_uid);
    pr_info("created init process: %s", init->name);

    scheduler();
    MOS_UNREACHABLE();
}
