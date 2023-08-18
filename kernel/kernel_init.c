// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/misc/power.h"

#include <mos/cmdline.h>
#include <mos/device/console.h>
#include <mos/elf/elf.h>
#include <mos/filesystem/vfs.h>
#include <mos/interrupt/ipi.h>
#include <mos/ipc/ipc.h>
#include <mos/lib/cmdline.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/setup.h>
#include <mos/tasks/kthread.h>
#include <mos/tasks/schedule.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_INIT_PATH "/initrd/programs/init"

typedef void (*init_function_t)(void);

static argv_t init_argv = { 0 };

static bool init_sysfs_argv(sysfs_file_t *file)
{
    for (u32 i = 0; i < init_argv.argc; i++)
        sysfs_printf(file, "%s ", init_argv.argv[i]);
    sysfs_printf(file, "\n");
    return true;
}

SYSFS_ITEM_RO_STRING(kernel_sysfs_version, MOS_KERNEL_VERSION)
SYSFS_ITEM_RO_STRING(kernel_sysfs_revision, MOS_KERNEL_REVISION)
SYSFS_ITEM_RO_STRING(kernel_sysfs_build_date, __DATE__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_build_time, __TIME__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_compiler, __VERSION__)
SYSFS_ITEM_RO_STRING(init_sysfs_path, init_argv.argv[0])

static const sysfs_item_t kernel_sysfs_items[] = {
    SYSFS_RO_ITEM("kernel_version", kernel_sysfs_version),
    SYSFS_RO_ITEM("kernel_revision", kernel_sysfs_revision),
    SYSFS_RO_ITEM("kernel_build_date", kernel_sysfs_build_date),
    SYSFS_RO_ITEM("kernel_build_time", kernel_sysfs_build_time),
    SYSFS_RO_ITEM("kernel_compiler", kernel_sysfs_compiler),
    SYSFS_RO_ITEM("init_path", init_sysfs_path),
    SYSFS_RO_ITEM("init_argv", init_sysfs_argv),
    SYSFS_END_ITEM,
};

SYSFS_AUTOREGISTER(kernel, kernel_sysfs_items);

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
MOS_SETUP("init", setup_init_path);

static bool setup_init_args(const char *arg)
{
    char *var_arg = strdup(arg);
    string_unquote(var_arg);
    init_argv.argv = cmdline_parse(init_argv.argv, var_arg, strlen(var_arg), &init_argv.argc);
    kfree(var_arg);
    return true;
}
MOS_SETUP("init_args", setup_init_args);

void mos_start_kernel(void)
{
    pr_info("Welcome to MOS!");
    pr_emph("MOS %s (%s), compiler version %s, on %s", MOS_KERNEL_VERSION, MOS_KERNEL_REVISION, __VERSION__, __DATE__);

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
    setup_invoke_setup();

    pr_emph("init path: %s", init_argv.argv[0]);
    for (u32 i = 1; i < init_argv.argc; i++)
        pr_emph("init arg %d: %s", i, init_argv.argv[i]);

    // register slab caches
    // this must be done before any other subsystems are initialized
    setup_reach_init_target(INIT_TARGET_SLAB_AUTOINIT);

    // power management
    setup_reach_init_target(INIT_TARGET_POWER);

    // register builtin filesystems
    vfs_init();
    setup_reach_init_target(INIT_TARGET_VFS);
    setup_reach_init_target(INIT_TARGET_SYSFS);

    bool mounted = vfs_mount("none", "/", "tmpfs", NULL);
    if (!mounted)
        mos_panic("failed to mount rootfs");

    vfs_mkdir("/initrd");
    bool mounted_initrd = vfs_mount("none", "/initrd/", "cpiofs", NULL);
    if (!mounted_initrd)
        mos_panic("failed to mount initrd");

    ipc_init();
    tasks_init();

    console_t *init_con = console_get("serial_com1");
    const stdio_t init_io = { .in = &init_con->io, .out = &init_con->io, .err = &init_con->io };
    process_t *init = elf_create_process(init_argv.argv[0], NULL, init_argv, &init_io);
    if (unlikely(!init))
        mos_panic("failed to create init process");

    pr_info("created init process: %s", init->name);

    kthread_init(); // must be called after creating the first init process
    setup_reach_init_target(INIT_TARGET_KTHREAD);

    ipi_init();

    unblock_scheduler();
    scheduler();
    MOS_UNREACHABLE();
}
