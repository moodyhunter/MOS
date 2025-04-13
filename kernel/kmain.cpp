// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/paging/paging.hpp"
#include "mos/mm/physical/pmm.hpp"
#include "mos/syslog/debug.hpp"
#include "mos/syslog/syslog.hpp"
#include "mos/tasks/elf.hpp"

#include <mos/allocator.hpp>
#include <mos/device/console.hpp>
#include <mos/filesystem/vfs.hpp>
#include <mos/interrupt/ipi.hpp>
#include <mos/ipc/ipc.hpp>
#include <mos/lib/cmdline.hpp>
#include <mos/list.hpp>
#include <mos/misc/cmdline.hpp>
#include <mos/misc/setup.hpp>
#include <mos/platform/platform.hpp>
#include <mos/shared_ptr.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/kthread.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/type_utils.hpp>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

MMContext mos_kernel_mm;

mos::vector<mos::string> init_args;

static bool init_sysfs_argv(sysfs_file_t *file)
{
    for (u32 i = 0; i < init_args.size(); i++)
        sysfs_printf(file, "%s ", init_args[i].c_str());
    sysfs_printf(file, "\n");
    return true;
}

SYSFS_ITEM_RO_STRING(kernel_sysfs_version, MOS_KERNEL_VERSION)
SYSFS_ITEM_RO_STRING(kernel_sysfs_revision, MOS_KERNEL_REVISION)
SYSFS_ITEM_RO_STRING(kernel_sysfs_build_date, __DATE__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_build_time, __TIME__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_compiler, __VERSION__)
SYSFS_ITEM_RO_STRING(kernel_sysfs_arch, MOS_ARCH)
SYSFS_ITEM_RO_STRING(init_sysfs_path, init_args[0].c_str())
SYSFS_ITEM_RO_PRINTF(initrd_sysfs_info, "pfn: " PFN_FMT "\nnpages: %zu\n", platform_info->initrd_pfn, platform_info->initrd_npages)

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
    if (arg.empty())
    {
        pr_warn("init path not specified");
        return false;
    }

    if (init_args.empty())
        init_args.push_back(arg.data());
    else
        init_args[0] = arg;

    return true;
}

MOS_SETUP("init_args", setup_init_args)
{
    char *var_arg = strdup(arg.data());
    string_unquote(var_arg);
    init_args = cmdline_parse_vector(var_arg, strlen(var_arg));
    kfree(var_arg);
    return true;
}

static void setup_sane_environment()
{
    platform_startup_early();
    pmm_init();

    pr_dinfo(vmm, "initializing paging...");
    mos_kernel_mm.pgd = pgd_create(pml_create_table(MOS_PMLTOP));
    platform_startup_setup_kernel_mm();

    pr_dinfo(vmm, "mapping kernel space...");
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
    slab_init(); // now mos::create<T>, kmalloc<T>, etc. are available
}

void mos_start_kernel(void)
{
    setup_sane_environment();
    mInfo << "Welcome to MOS!";
    mInfo << fmt("MOS {}-{} on ({}, {}), compiler {}", MOS_KERNEL_VERSION, MOS_ARCH, MOS_KERNEL_REVISION, __DATE__, __VERSION__);

    if (platform_info->n_cmdlines)
    {
        mInfo << "MOS Kernel cmdline";
        for (u32 i = 0; i < platform_info->n_cmdlines; i++)
        {
            const cmdline_option_t *opt = &platform_info->cmdlines[i];
            if (opt->arg)
                pr_info2("  %-2d: %-10s = %s", i, opt->name, opt->arg);
            else
                pr_info2("  %-2d: %s", i, opt->name);
        }
    }

    // power management
    startup_invoke_autoinit(INIT_TARGET_POWER);

    // register builtin filesystems
    startup_invoke_autoinit(INIT_TARGET_PRE_VFS);
    startup_invoke_autoinit(INIT_TARGET_VFS);
    startup_invoke_autoinit(INIT_TARGET_SYSFS);

    platform_startup_late();

    init_args.push_back(MOS_DEFAULT_INIT_PATH);
    startup_invoke_cmdline_hooks();
    {
        const auto ret = vfs_mount("none", "/", "tmpfs", NULL);
        if (ret.isErr())
            mos_panic("failed to mount rootfs, vfs_mount returns %ld", ret.getErr());
    }
    {
        const auto ret = vfs_mkdir("/initrd");
        if (ret.isErr())
            mos_panic("failed to create /initrd, vfs_mkdir returns %ld", ret.getErr());
    }
    {
        const auto ret = vfs_mount("none", "/initrd/", "cpiofs", NULL);
        if (ret.isErr())
            mos_panic("failed to mount initrd, vfs_mount returns %ld", ret.getErr());
    }
    ipc_init();
    scheduler_init();

    const auto init_con = platform_info->boot_console;
    if (unlikely(!init_con))
        mos_panic("failed to get console");

    const stdio_t init_io = { .in = init_con, .out = init_con, .err = init_con };
    const mos::vector<mos::string> init_envp = {
        "PATH=/initrd/programs:/initrd/bin:/bin",
        "HOME=/",
        "TERM=linux",
    };

    mInfo << "running '" << init_args[0] << "' as init process.";
    mInfo << "  with arguments:";
    for (u32 i = 0; i < init_args.size(); i++)
        mInfo << fmt("    argv[{}] = {}", i, init_args[i].c_str());
    mInfo << "  with environment:";
    for (u32 i = 0; i < init_envp.size(); i++)
        mInfo << "    " << init_envp[i].c_str();

    const auto init = elf_create_process(init_args[0], NULL, init_args, init_envp, &init_io);
    if (unlikely(!init))
        mos_panic("failed to create init process");

    const auto m = mm_map_user_pages(init->mm, MOS_INITRD_BASE, platform_info->initrd_pfn, platform_info->initrd_npages, VM_USER_RO, VMAP_TYPE_SHARED, VMAP_FILE, true);
    pmm_ref(platform_info->initrd_pfn, platform_info->initrd_npages);
    pmm_ref(platform_info->initrd_pfn, platform_info->initrd_npages);

    MOS_ASSERT_X(m, "failed to map initrd into init process");

    kthread_init(); // must be called after creating the first init process
    startup_invoke_autoinit(INIT_TARGET_KTHREAD);

    unblock_scheduler();

    mInfo << "\n";
    enter_scheduler();
    MOS_UNREACHABLE();
}
