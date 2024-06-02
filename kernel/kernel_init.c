// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/mm.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/physical/pmm.h"

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
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

#if MOS_DEBUG_FEATURE(vmm)
#include "mos/mm/paging/dump.h"
#include "mos/panic.h" // for panic_hook_declare and panic_hook_install
#endif

static void invoke_constructors(void)
{
    typedef void (*init_function_t)(void);
    extern const init_function_t __init_array_start[], __init_array_end;

    pr_dinfo2(setup, "invoking constructors...");
    for (const init_function_t *func = __init_array_start; func != &__init_array_end; func++)
    {
        pr_dinfo2(setup, "  %ps", (void *) (ptr_t) *func);
        (*func)();
    }
}
MOS_INIT(POST_MM, invoke_constructors);

static struct
{
    size_t argc; // size of argv, does not include the terminating NULL
    const char **argv;
} init_args = { 0 };

static bool init_sysfs_argv(sysfs_file_t *file)
{
    for (u32 i = 0; i < init_args.argc; i++)
        sysfs_printf(file, "%s ", init_args.argv[i]);
    sysfs_printf(file, "\n");
    return true;
}

SYSFS_ITEM_RO_STRING(kernel_sysfs_version, MOS_KERNEL_VERSION)
SYSFS_ITEM_RO_STRING(kernel_sysfs_revision, MOS_KERNEL_REVISION)
SYSFS_ITEM_RO_STRING(kernel_sysfs_build_date, __DATE__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_build_time, __TIME__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_compiler, __VERSION__)
SYSFS_ITEM_RO_STRING(init_sysfs_path, init_args.argv[0])

static sysfs_item_t kernel_sysfs_items[] = {
    SYSFS_RO_ITEM("version", kernel_sysfs_version),       //
    SYSFS_RO_ITEM("revision", kernel_sysfs_revision),     //
    SYSFS_RO_ITEM("build_date", kernel_sysfs_build_date), //
    SYSFS_RO_ITEM("build_time", kernel_sysfs_build_time), //
    SYSFS_RO_ITEM("compiler", kernel_sysfs_compiler),     //
    SYSFS_RO_ITEM("init_path", init_sysfs_path),          //
    SYSFS_RO_ITEM("init_argv", init_sysfs_argv),          //
};

SYSFS_AUTOREGISTER(kernel, kernel_sysfs_items);

static bool setup_init_path(const char *arg)
{
    if (!arg)
    {
        pr_warn("init path not specified");
        return false;
    }

    if (init_args.argv)
        kfree(init_args.argv[0]); // free the old init path

    init_args.argv[0] = strdup(arg);
    return true;
}
MOS_SETUP("init", setup_init_path);

static bool setup_init_args(const char *arg)
{
    char *var_arg = strdup(arg);
    string_unquote(var_arg);
    init_args.argv = cmdline_parse(init_args.argv, var_arg, strlen(var_arg), &init_args.argc);
    kfree(var_arg);
    return true;
}
MOS_SETUP("init_args", setup_init_args);

void mos_start_kernel(void)
{
    pr_info("Welcome to MOS!");
    pr_emph("MOS %s (%s), compiler version %s, on %s", MOS_KERNEL_VERSION, MOS_KERNEL_REVISION, __VERSION__, __DATE__);

    platform_startup_early();

    if (platform_info->n_cmdlines)
        pr_emph("MOS Kernel cmdline");

    for (u32 i = 0; i < platform_info->n_cmdlines; i++)
    {
        const cmdline_option_t *opt = &platform_info->cmdlines[i];
        if (opt->arg)
            pr_info2("  %-2d: %-10s = %s", i, opt->name, opt->arg);
        else
            pr_info2("  %-2d: %s", i, opt->name);
    }

    platform_startup_mm();
#if MOS_DEBUG_FEATURE(vmm)
    panic_hook_declare(mm_dump_current_pagetable, "dump current pagetable");
    panic_hook_install(&mm_dump_current_pagetable_holder);
#endif
    startup_invoke_autoinit(INIT_TARGET_POST_MM);
    startup_invoke_autoinit(INIT_TARGET_SLAB_AUTOINIT);

    // power management
    startup_invoke_autoinit(INIT_TARGET_POWER);

    // register builtin filesystems
    startup_invoke_autoinit(INIT_TARGET_PRE_VFS);
    startup_invoke_autoinit(INIT_TARGET_VFS);
    startup_invoke_autoinit(INIT_TARGET_SYSFS);

    platform_startup_late();

    init_args.argc = 1;
    init_args.argv = kcalloc(1, sizeof(char *)); // init_argv[0] is the init path
    init_args.argv[0] = strdup(MOS_DEFAULT_INIT_PATH);
    startup_invoke_setup();
    init_args.argv = krealloc(init_args.argv, (init_args.argc + 1) * sizeof(char *));
    init_args.argv[init_args.argc] = NULL;

    long ret = vfs_mount("none", "/", "tmpfs", NULL);
    if (IS_ERR_VALUE(ret))
        mos_panic("failed to mount rootfs");

    vfs_mkdir("/initrd");
    ret = vfs_mount("none", "/initrd/", "cpiofs", NULL);
    if (IS_ERR_VALUE(ret))
        mos_panic("failed to mount initrd");

    ipc_init();
    tasks_init();

    console_t *const init_con = console_get("serial_com1");
    const stdio_t init_io = { .in = &init_con->io, .out = &init_con->io, .err = &init_con->io };
    const char *const init_envp[] = {
        "PATH=/initrd/programs:/initrd/bin:/bin",
        "HOME=/",
        "TERM=linux",
        NULL,
    };

    pr_info("run '%s' as init process", init_args.argv[0]);
    pr_info2("  with arguments:");
    for (u32 i = 0; i < init_args.argc; i++)
        pr_info2("    argv[%d] = %s", i, init_args.argv[i]);
    pr_info2("  with environment:");
    for (u32 i = 0; init_envp[i]; i++)
        pr_info2("    %s", init_envp[i]);

    process_t *init = elf_create_process(init_args.argv[0], NULL, init_args.argv, init_envp, &init_io);
    if (unlikely(!init))
        mos_panic("failed to create init process");

// map initrd into init process
#if MOS_CONFIG(MOS_MAP_INITRD_TO_INIT)
    vmap_t *initrd_map = mm_map_user_pages( //
        init->mm,                           //
        MOS_INITRD_BASE,                    //
        platform_info->initrd_pfn,          //
        platform_info->initrd_npages,       //
        VM_USER_RO,                         //
        VALLOC_EXACT,                       //
        VMAP_TYPE_SHARED,                   //
        VMAP_FILE                           //
    );

    pmm_ref(platform_info->initrd_pfn, platform_info->initrd_npages);
    pmm_ref(platform_info->initrd_pfn, platform_info->initrd_npages);

    MOS_ASSERT_X(initrd_map, "failed to map initrd into init process");
#endif

    kthread_init(); // must be called after creating the first init process
    startup_invoke_autoinit(INIT_TARGET_KTHREAD);

    unblock_scheduler();

    pr_cont("\n");
    scheduler();
    MOS_UNREACHABLE();
}
