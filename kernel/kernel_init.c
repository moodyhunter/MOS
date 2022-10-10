// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/cmdline.h"
#include "mos/device/block.h"
#include "mos/elf/elf.h"
#include "mos/filesystem/cpio/cpio.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/pathutils.h"
#include "mos/mm/paging.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"
#include "mos/x86/tasks/tss_types.h"

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

    pr_info("mounted initrd to rootfs");
    return true;
}

const char *get_init_path(void)
{
    cmdline_arg_t *init_arg = mos_cmdline_get_arg("init");
    if (init_arg && init_arg->params_count > 0)
    {
        cmdline_param_t *init_param = init_arg->params[0];
        if (init_param->param_type == CMDLINE_PARAM_TYPE_STRING)
            return init_param->val.string;

        pr_warn("init path is not a string, using default '/init'");
    }
    return "/init";
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
    pr_emph("%-25s'%s'", "Kernel builtin cmdline:", MOS_KERNEL_BUILTIN_CMDLINE);
}

void mos_start_kernel(const char *cmdline)
{
    mos_cmdline = mos_cmdline_create(cmdline);

    pr_info("Welcome to MOS!");
    dump_cmdline();

#if MOS_MEME
    mos_warn("V2Ray 4.45.2 started");
#endif

    if (mos_cmdline_get_arg("mos_tests"))
        mos_test_engine_run_tests();

    process_init();
    thread_init();

    if (unlikely(!mount_initrd()))
        mos_panic("failed to mount initrd");

    const char *init_path = get_init_path();
    file_t *init_file = vfs_open(init_path, OPEN_READ);
    if (!init_file)
        mos_panic("failed to open init");

    size_t npage_required = init_file->io.size / MOS_PAGE_SIZE + 1;
    char *initrd_data = kpage_alloc(npage_required, PGALLOC_NONE);
    size_t r = io_read(&init_file->io, initrd_data, init_file->io.size);
    MOS_ASSERT_X(r == init_file->io.size, "failed to read init");

    elf_header_t *header = (elf_header_t *) initrd_data;
    elf_verify_result verify_result = mos_elf_verify_header(header);
    if (verify_result != ELF_VERIFY_OK)
    {
        switch (verify_result)
        {
            case ELF_VERIFY_INVALID_ENDIAN: pr_emerg("Invalid ELF endianness"); break;
            case ELF_VERIFY_INVALID_MAGIC: pr_emerg("Invalid ELF magic"); break;
            case ELF_VERIFY_INVALID_MAGIC_ELF: pr_emerg("Invalid ELF magic: 'ELF'"); break;
            case ELF_VERIFY_INVALID_VERSION: pr_emerg("Invalid ELF version"); break;
            case ELF_VERIFY_INVALID_BITS: pr_emerg("Invalid ELF bits"); break;
            case ELF_VERIFY_INVALID_OSABI: pr_emerg("Invalid ELF OS ABI"); break;
            default: MOS_UNREACHABLE();
        }

        mos_panic("failed to verify 'init' ELF header for '%s'", init_path);
    }

    if (header->object_type != ELF_OBJECT_EXECUTABLE)
        mos_panic("'init' is not an executable");

    elf_program_header_t *init_ph = (elf_program_header_t *) (initrd_data + header->program_header_offset);
    elf_section_header_t *init_sh = (elf_section_header_t *) (initrd_data + header->section_header_offset);

    int i_program = 1, i_section = 1;
    while (init_ph->header_type != ELF_PH_T_LOAD && i_program < header->program_header.count)
        init_ph++, i_program++;
    while (init_sh->header_type != ELF_SH_T_PROGBITS && i_section < header->section_header.count)
        init_sh++, i_section++;

    MOS_ASSERT_X(init_sh->header_type == ELF_SH_T_PROGBITS, "no code section found");
    MOS_ASSERT_X(init_ph->header_type == ELF_PH_T_LOAD, "no loadable segment found");

    // the stack memory to be used if we enter the kernelmode by a trap / interrupt
    // TODO: Per-process stack
    extern tss32_t tss_entry;
    tss_entry.esp0 = (u32) kpage_alloc(1, PGALLOC_NONE) + MOS_PAGE_SIZE; // stack grows downwards from the top of the page
    pr_emph("kernel stack at " PTR_FMT, (uintptr_t) tss_entry.esp0);

    process_t *pinit = create_process((process_id_t){ 1 }, (uid_t){ 0 }, (thread_entry_t) header->entry_point, (void *) 114514);

    mos_platform->mm_map_kvaddr(pinit->pagetable, init_ph->vaddr, (uintptr_t) header + init_ph->data_offset, 1, VM_USERMODE);

    scheduler();
    MOS_UNREACHABLE();
}
