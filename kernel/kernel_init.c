// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/cmdline.h>
#include <mos/device/console.h>
#include <mos/elf/elf.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/interrupt/ipi.h>
#include <mos/io/terminal.h>
#include <mos/ipc/ipc.h>
#include <mos/kconfig.h>
#include <mos/lib/cmdline.h>
#include <mos/mm/kmalloc.h>
#include <mos/mm/shm.h>
#include <mos/printk.h>
#include <mos/tasks/kthread.h>
#include <mos/tasks/schedule.h>
#include <string.h>

extern filesystem_t fs_tmpfs;
extern filesystem_t fs_cpiofs;

#define DEFAULT_INIT_PATH "/initrd/programs/init"
// const char *init_path = DEFAULT_INIT_PATH;
// const char **init_argv = NULL;

static argv_t init_argv = { 0 };

static bool setup_init_path(const char *arg)
{
    if (!arg)
    {
        pr_warn("init path not specified");
        return false;
    }

    if (init_argv.argv)
        kfree(init_argv.argv[0]); // free the old init path

    init_argv.argv[0] = strdup(arg);
    return true;
}
__setup("init", setup_init_path);

static bool setup_init_args(const char *arg)
{
    char *var_arg = strdup(arg);
    string_unquote(var_arg);
    init_argv.argv = cmdline_parse(init_argv.argv, var_arg, strlen(var_arg), &init_argv.argc);
    kfree(var_arg);
    return true;
}
__setup("init_args", setup_init_args);

void mos_start_kernel(void)
{
    pr_info("Welcome to MOS!");
    pr_emph("MOS %s (%s), compiler version %s, on %s", MOS_KERNEL_VERSION, MOS_KERNEL_REVISION_STRING, __VERSION__, __DATE__);

    if (mos_cmdlines_count)
        pr_emph("MOS Kernel cmdline");

    for (u32 i = 0; i < mos_cmdlines_count; i++)
    {
        const cmdline_option_t *opt = &mos_cmdlines[i];
        if (opt->arg)
            pr_info2("  %-2d: %-10s = %s", i, opt->name, opt->arg);
        else
            pr_info2("  %-2d: %s", i, opt->name);
    }

    init_argv.argc = 1;
    init_argv.argv = kcalloc(1, sizeof(char *)); // init_argv[0] is the init path
    init_argv.argv[0] = strdup(DEFAULT_INIT_PATH);
    mos_cmdline_do_setup();

    pr_emph("init path: %s", init_argv.argv[0]);
    for (u32 i = 1; i < init_argv.argc; i++)
        pr_emph("init arg %d: %s", i, init_argv.argv[i]);

    // register builtin filesystems
    vfs_init();
    vfs_register_filesystem(&fs_tmpfs);
    vfs_register_filesystem(&fs_cpiofs);

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
    process_t *init = elf_create_process(init_argv.argv[0], NULL, init_term, init_argv);
    if (unlikely(!init))
        mos_panic("failed to create init process");

    pr_info("created init process: %s", init->name);

    kthread_init(); // must be called after creating the first init process

    ipi_init();

    unblock_scheduler();
    scheduler();
    MOS_UNREACHABLE();
}
