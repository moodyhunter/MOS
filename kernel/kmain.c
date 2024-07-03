// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/mm.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/physical/pmm.h"
#include "mos/tasks/elf.h"

#include <mos/device/console.h>
#include <mos/filesystem/vfs.h>
#include <mos/interrupt/ipi.h>
#include <mos/ipc/ipc.h>
#include <mos/lib/cmdline.h>
#include <mos/misc/cmdline.h>
#include <mos/misc/setup.h>
#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
#include <mos/tasks/kthread.h>
#include <mos/tasks/schedule.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

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

static bool initrd_sysfs_info(sysfs_file_t *f)
{
    sysfs_printf(f, "pfn: " PFN_FMT "\nnpages: %zu\n", platform_info->initrd_pfn, platform_info->initrd_npages);
    return true;
}

SYSFS_ITEM_RO_STRING(kernel_sysfs_version, MOS_KERNEL_VERSION)
SYSFS_ITEM_RO_STRING(kernel_sysfs_revision, MOS_KERNEL_REVISION)
SYSFS_ITEM_RO_STRING(kernel_sysfs_build_date, __DATE__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_build_time, __TIME__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_compiler, __VERSION__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_arch, MOS_ARCH)
SYSFS_ITEM_RO_STRING(init_sysfs_path, init_args.argv[0])

static sysfs_item_t kernel_sysfs_items[] = {
    SYSFS_RO_ITEM("arch", kernel_sysfs_arch),             //
    SYSFS_RO_ITEM("build_date", kernel_sysfs_build_date), //
    SYSFS_RO_ITEM("build_time", kernel_sysfs_build_time), //
    SYSFS_RO_ITEM("compiler", kernel_sysfs_compiler),     //
    SYSFS_RO_ITEM("init_argv", init_sysfs_argv),          //
    SYSFS_RO_ITEM("init_path", init_sysfs_path),          //
    SYSFS_RO_ITEM("initrd", initrd_sysfs_info),           //
    SYSFS_RO_ITEM("revision", kernel_sysfs_revision),     //
    SYSFS_RO_ITEM("version", kernel_sysfs_version),       //
};

SYSFS_AUTOREGISTER(kernel, kernel_sysfs_items);

MOS_SETUP("init", setup_init_path)
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

MOS_SETUP("init_args", setup_init_args)
{
    char *var_arg = strdup(arg);
    string_unquote(var_arg);
    init_args.argv = cmdline_parse(init_args.argv, var_arg, strlen(var_arg), &init_args.argc);
    kfree(var_arg);
    return true;
}

void mos_start_kernel(void)
{
    pr_info("Welcome to MOS!");
    pr_emph("MOS %s on %s (%s, %s), compiler %s", MOS_KERNEL_VERSION, MOS_ARCH, MOS_KERNEL_REVISION, __DATE__, __VERSION__);

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

    platform_startup_early();
    pmm_init();
    platform_startup_setup_kernel_mm();

    pr_dinfo2(vmm, "mapping kernel space...");

    mm_map_kernel_pages(                                                                                   //
        platform_info->kernel_mm,                                                                          //
        (ptr_t) __MOS_KERNEL_CODE_START,                                                                   //
        MOS_KERNEL_PFN((ptr_t) __MOS_KERNEL_CODE_START),                                                   //
        ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_CODE_END - (ptr_t) __MOS_KERNEL_CODE_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_EXEC | VM_GLOBAL                                                                      //
    );

    mm_map_kernel_pages(                                                                                       //
        platform_info->kernel_mm,                                                                              //
        (ptr_t) __MOS_KERNEL_RODATA_START,                                                                     //
        MOS_KERNEL_PFN((ptr_t) __MOS_KERNEL_RODATA_START),                                                     //
        ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_RODATA_END - (ptr_t) __MOS_KERNEL_RODATA_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_GLOBAL                                                                                    //
    );

    mm_map_kernel_pages(                                                                               //
        platform_info->kernel_mm,                                                                      //
        (ptr_t) __MOS_KERNEL_RW_START,                                                                 //
        MOS_KERNEL_PFN((ptr_t) __MOS_KERNEL_RW_START),                                                 //
        ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_RW_END - (ptr_t) __MOS_KERNEL_RW_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_WRITE | VM_GLOBAL                                                                 //
    );

    platform_switch_mm(platform_info->kernel_mm);

    startup_invoke_autoinit(INIT_TARGET_POST_MM);
    startup_invoke_autoinit(INIT_TARGET_SLAB_AUTOINIT);

    // power management
    startup_invoke_autoinit(INIT_TARGET_POWER);

    // register builtin filesystems
    startup_invoke_autoinit(INIT_TARGET_PRE_VFS);
    startup_invoke_autoinit(INIT_TARGET_VFS);
    startup_invoke_autoinit(INIT_TARGET_SYSFS);

    platform_startup_late();
    invoke_constructors();

    init_args.argc = 1;
    init_args.argv = kcalloc(1, sizeof(char *)); // init_argv[0] is the init path
    init_args.argv[0] = strdup(MOS_DEFAULT_INIT_PATH);
    startup_invoke_cmdline_hooks();
    init_args.argv = krealloc(init_args.argv, (init_args.argc + 1) * sizeof(char *));
    init_args.argv[init_args.argc] = NULL;

    long ret = vfs_mount("none", "/", "tmpfs", NULL);
    if (IS_ERR_VALUE(ret))
        mos_panic("failed to mount rootfs, vfs_mount returns %ld", ret);

    vfs_mkdir("/initrd");
    ret = vfs_mount("none", "/initrd/", "cpiofs", NULL);
    if (IS_ERR_VALUE(ret))
        mos_panic("failed to mount initrd, vfs_mount returns %ld", ret);

    ipc_init();
    tasks_init();
    scheduler_init();

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

    kthread_init(); // must be called after creating the first init process
    startup_invoke_autoinit(INIT_TARGET_KTHREAD);

    unblock_scheduler();

    pr_cont("\n");
    enter_scheduler();
    MOS_UNREACHABLE();
}
