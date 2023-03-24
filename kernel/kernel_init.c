// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/device/console.h>
#include <mos/device/device_manager.h>
#include <mos/elf/elf.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/io/terminal.h>
#include <mos/ipc/ipc.h>
#include <mos/kallsyms.h>
#include <mos/kconfig.h>
#include <mos/mm/kmalloc.h>
#include <mos/mm/shm.h>
#include <mos/printk.h>
#include <mos/setup.h>
#include <mos/tasks/kthread.h>
#include <mos/tasks/schedule.h>
#include <string.h>

extern filesystem_t fs_tmpfs;
extern filesystem_t fs_cpiofs;
extern filesystem_t fs_ipcfs;
const char *init_path = "/initrd/programs/init";

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

__setup("init", setup_init_path);

argv_t create_argv_from_cmdline(void)
{
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

    init_argv.argv[init_argv.argc] = NULL;
    return init_argv;
}

void mos_start_kernel(const char *cmdline)
{
    mos_cmdline = cmdline_create(cmdline);

    pr_info("Welcome to MOS!");
    pr_emph("MOS %s (%s)", MOS_KERNEL_VERSION, MOS_KERNEL_REVISION_STRING);

    if (mos_cmdline->options_count)
        pr_emph("MOS Kernel cmdline");

    for (u32 i = 0; i < mos_cmdline->options_count; i++)
    {
        cmdline_option_t *option = mos_cmdline->options[i];
        pr_info("%2d: %s", i + 1, option->name);
        for (u32 j = 0; j < option->argc; j++)
            pr_info2("  %2d: %s", j + 1, option->argv[j]);
    }

    invoke_setup_functions(mos_cmdline);

    // register builtin filesystems
    vfs_init();
    vfs_register_filesystem(&fs_tmpfs);
    vfs_register_filesystem(&fs_cpiofs);
    vfs_register_filesystem(&fs_ipcfs);

    bool mounted = vfs_mount("none", "/", "tmpfs", NULL);
    if (!mounted)
        mos_panic("failed to mount rootfs");

    vfs_mkdir("/initrd");
    bool mounted_initrd = vfs_mount("initrd", "/initrd/", "cpiofs", NULL);
    if (!mounted_initrd)
        mos_panic("failed to mount initrd");

    ipc_init();
    tasks_init();

    console_t *init_con = console_get("serial_com1");
    if (init_con->caps & CONSOLE_CAP_CLEAR)
        init_con->clear(init_con);

    terminal_t *init_term = terminal_create_console(init_con);

    const argv_t init_argv = create_argv_from_cmdline();
    process_t *init = elf_create_process(init_path, NULL, init_term, init_argv);
    if (unlikely(!init))
        mos_panic("failed to create init process");

    pr_info("created init process: %s", init->name);

    kthread_init(); // must be called after creating the first init process
    device_manager_init();

    pr_info("starting scheduler");
    unblock_scheduler();
    scheduler();
    MOS_UNREACHABLE();
}
