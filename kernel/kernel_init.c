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

elf_header_t *open_elf_executable(const char *path)
{
    file_t *f = vfs_open(path, FILE_OPEN_READ);
    if (!f)
    {
        pr_emerg("failed to open '%s'", path);
        goto bail_out;
    }

    size_t npage_required = f->io.size / MOS_PAGE_SIZE + 1;
    char *header_buf = kpage_alloc(npage_required, PGALLOC_NONE);

    size_t size = io_read(&f->io, header_buf, f->io.size);
    MOS_ASSERT_X(size == f->io.size, "failed to read init");

    elf_header_t *elf_header = (elf_header_t *) header_buf;
    if (elf_header->object_type != ELF_OBJTYPE_EXECUTABLE)
    {
        pr_emerg("'%s' is not an executable", path);
        goto bail_out;
    }

    elf_verify_result verify_result = elf_verify_header(elf_header);
    if (verify_result != ELF_VERIFY_OK)
    {
        pr_emerg("failed to verify ELF header for '%s', result: %d", path, (int) verify_result);
        goto bail_out;
    }

    return elf_header;

bail_out:
    if (f)
        io_close(&f->io);
    return NULL;
}

process_t *create_elf_process(elf_header_t *elf)
{
    const char *buf = (const char *) elf;

    process_t *proc = allocate_process((process_id_t){ 1 }, (uid_t){ 0 }, (thread_entry_t) elf->entry_point, NULL);

    for (int i = 0; i < elf->ph.count; i++)
    {
        elf_program_hdr_t *ph = (elf_program_hdr_t *) (buf + elf->ph_offset + i * elf->ph.entry_size);
        pr_info("program header %d: %c%c%c%s at " PTR_FMT, i,
                (elf_program_header_flags) ph->p_flags & ELF_PH_F_R ? 'r' : '-', //
                (elf_program_header_flags) ph->p_flags & ELF_PH_F_W ? 'w' : '-', //
                (elf_program_header_flags) ph->p_flags & ELF_PH_F_X ? 'x' : '-', //
                ph->header_type == ELF_PH_T_LOAD ? " (load)" : "",               //
                ph->vaddr                                                        //
        );

        if (!(ph->header_type & ELF_PH_T_LOAD))
            continue;

        vm_flags map_flags = (                            //
            (ph->p_flags & ELF_PH_F_R ? VM_READ : 0) |    //
            (ph->p_flags & ELF_PH_F_W ? VM_WRITE : 0) |   //
            (ph->p_flags & ELF_PH_F_X ? VM_EXECUTE : 0) | //
            VM_USERMODE                                   //
        );

        mos_platform->mm_map_kvaddr(                              //
            proc->pagetable,                                      //
            ALIGN_DOWN_TO_PAGE(ph->vaddr),                        //
            (uintptr_t) buf + ph->data_offset,                    //
            ALIGN_UP_TO_PAGE(ph->segsize_in_mem) / MOS_PAGE_SIZE, //
            map_flags                                             //
        );
    }

    const char *const strtab = buf + ((elf_section_hdr_t *) (buf + elf->sh_offset + elf->sh_strtab_index * elf->sh.entry_size))->sh_offset;
    for (int i = 0; i < elf->sh.count; i++)
    {
        elf_section_hdr_t *sh = (elf_section_hdr_t *) (buf + elf->sh_offset + i * elf->sh.entry_size);
        pr_info2("section %2d: %s", i, strtab + sh->name_index);
    }

    return proc;
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

    const char *init_path = cmdline_get_init_path();

    elf_header_t *init_header = open_elf_executable(init_path);
    if (!init_header)
        mos_panic("failed to open init");
    process_t *init = create_elf_process(init_header);
    MOS_UNUSED(init);

    // the stack memory to be used if we enter the kernelmode by a trap / interrupt
    // TODO: Per-process stack
    extern tss32_t tss_entry;
    tss_entry.esp0 = (u32) kpage_alloc(1, PGALLOC_NONE) + 1 * MOS_PAGE_SIZE; // stack grows downwards from the top of the page
    pr_emph("kernel stack at " PTR_FMT, (uintptr_t) tss_entry.esp0);

    scheduler();
    MOS_UNREACHABLE();
}
