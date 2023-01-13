// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/device/console.h"
#include "mos/device/device_manager.h"
#include "mos/elf/elf.h"
#include "mos/filesystem/cpio/cpio.h"
#include "mos/filesystem/pathutils.h"
#include "mos/io/terminal.h"
#include "mos/ipc/ipc.h"
#include "mos/kconfig.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#include "mos/setup.h"
#include "mos/tasks/kthread.h"
#include "mos/tasks/process.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/thread.h"

const char *init_path = "/programs/init";

bool setup_init_path(int argc, const char **argv)
{
    if (argc != 1)
    {
        pr_warn("setup_init_path: expected 1 argument, got %d", argc);
        for (int i = 0; i < argc; i++)
            pr_warn("  %d: %s", i, argv[i]);
        return false;
    }

    init_path = strdup(argv[0]);
    pr_emph("init path set to '%s'", init_path);
    return true;
}

__setup(init, "init", setup_init_path);

bool mount_initrd(void)
{
    blockdev_t *dev = blockdev_find("initrd");
    if (!dev)
        return false;
    pr_info2("found initrd block device: %s", dev->name);

    mountpoint_t *mount = kmount(&root_path, &fs_cpio, dev);
    if (!mount)
        return false;

    pr_info2("mounted initrd as rootfs");
    return true;
}

void mos_start_kernel(const char *cmdline)
{
    mos_cmdline = cmdline_create(cmdline);

    pr_info("Welcome to MOS!");
    pr_emph("MOS %s (%s)", MOS_KERNEL_VERSION, MOS_KERNEL_REVISION_STRING);

    if (mos_cmdline->options_count)
        pr_emph("MOS Arguments: (total of %zu options)", mos_cmdline->options_count);

    for (u32 i = 0; i < mos_cmdline->options_count; i++)
    {
        cmdline_option_t *option = mos_cmdline->options[i];
        pr_info("%2d: %s", i + 1, option->name);
        for (u32 j = 0; j < option->argc; j++)
            pr_info2("  %2d: %s", j + 1, option->argv[j]);
    }

    invoke_setup_functions(mos_cmdline);

    ipc_init();
    process_init();
    thread_init();

    if (unlikely(!mount_initrd()))
        mos_panic("failed to mount initrd");

    console_t *init_con = console_get("x86");
    if (!init_con)
    {
        init_con = console_get_by_prefix("x86");
        if (!init_con)
            mos_panic("failed to find serial console");
    }

    if (init_con->caps & CONSOLE_CAP_CLEAR)
        init_con->clear(init_con);

    terminal_t *init_term = terminal_create_console(init_con);

    // construct argv for init process
    argv_t init_argv = {
        .argc = mos_cmdline->options_count + 1,
        .argv = kmalloc(sizeof(char *) * (mos_cmdline->options_count + 2)), // +1 for init path, +1 for NULL
    };

    init_argv.argv[0] = strdup(init_path);

    for (u32 i = 0; i < mos_cmdline->options_count; i++)
    {
        const cmdline_option_t *option = mos_cmdline->options[i];

        // concatenate all arguments into a single string
        size_t len = strlen(option->name) + 1; // +1 for '='
        for (u32 j = 0; j < option->argc; j++)
            len += strlen(option->argv[j]) + 1; // +1 for ',' (or '\0' for last argument)

        char *str = kmalloc(len);
        size_t offset = 0;
        strcpy(str, option->name);
        offset += strlen(option->name);

        if (option->argc)
        {
            str[offset++] = '=';
            for (u32 j = 0; j < option->argc; j++)
            {
                strcpy(str + offset, option->argv[j]);
                offset += strlen(option->argv[j]);
                if (j != option->argc - 1)
                    str[offset++] = ',';
            }
        }
        str[offset] = '\0'; // null-terminate the string
        init_argv.argv[i + 1] = str;
    }

    process_t *init = elf_create_process(init_path, NULL, init_term, (uid_t) 0, init_argv);
    pr_info("created init process: %s", init->name);

    kthread_init(); // must be called after creating the first init process
    device_manager_init();

    pr_info("starting scheduler");
    unblock_scheduler();
    scheduler();
    MOS_UNREACHABLE();
}
