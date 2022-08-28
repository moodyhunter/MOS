// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/hashmap.h"
#include "lib/structures/stack.h"
#include "mos/cmdline.h"
#include "mos/device/block.h"
#include "mos/elf/elf.h"
#include "mos/filesystem/cpio/cpio.h"
#include "mos/filesystem/filesystem.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/pathutils.h"
#include "mos/kconfig.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"
#include "mos/types.h"
#include "mos/x86/tasks/context.h"
#include "mos/x86/tasks/tss_types.h"
#include "mos/x86/x86_platform.h"

extern void mos_test_engine_run_tests(); // defined in tests/test_engine.c

void schedule();
extern void platform_context_switch(thread_t *old_thread, thread_t *new_thread);
asmlinkage void jump_to_usermain(uintptr_t, uintptr_t);

extern tss32_t tss_entry;

void mos_start_kernel(const char *cmdline)
{
    mos_cmdline = mos_cmdline_create(cmdline);

    pr_info("Welcome to MOS!");
    pr_emph("MOS %s (%s)", MOS_KERNEL_VERSION, MOS_KERNEL_REVISION);
    pr_emph("MOS Arguments: (total of %zu options)", mos_cmdline->args_count);
    for (u32 i = 0; i < mos_cmdline->args_count; i++)
    {
        cmdline_arg_t *option = mos_cmdline->arguments[i];
        pr_info("%2d: %s", i, option->arg_name);

        for (u32 j = 0; j < option->param_count; j++)
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

#if MOS_MEME
    mos_warn("V2Ray 4.45.2 started");
#endif

    process_init();
    thread_init();

    if (mos_cmdline_get_arg("mos_tests"))
    {
        mos_test_engine_run_tests();
    }

    const char *init_path = "/init";

    cmdline_arg_t *init_arg = mos_cmdline_get_arg("init");
    if (init_arg && init_arg->param_count > 0 && init_arg->params[0]->param_type == CMDLINE_PARAM_TYPE_STRING)
        init_path = init_arg->params[0]->val.string;

    blockdev_t *dev = blockdev_find("initrd");
    if (!dev)
        mos_panic("no initrd found");
    pr_info("found initrd block device: %s", dev->name);

    mountpoint_t *mount = kmount(&root_path, &fs_cpio, dev);
    if (!mount)
        mos_panic("failed to mount initrd");

    pr_info("Loading init program '%s'", init_path);

    file_t *init_file = vfs_open(init_path, OPEN_READ);
    if (!init_file)
        mos_panic("failed to open init");

    size_t npage_required = init_file->io.size / mos_platform.mm_page_size + 1;

    char *initrd_data = kpage_alloc(npage_required);
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

    elf_program_header_t *program_header __maybe_unused = (elf_program_header_t *) (initrd_data + header->program_header_offset);
    elf_section_header_t *section_header __maybe_unused = (elf_section_header_t *) (initrd_data + header->section_header_offset);

    int x = 1;
    while (program_header->header_type != ELF_PH_T_LOAD && x < header->program_header.count)
        program_header++, x++;

    x = 1;
    while (section_header->header_type != ELF_SH_T_PROGBITS && x < header->section_header.count)
        section_header++, x++;

    MOS_ASSERT_X(section_header->header_type == ELF_SH_T_PROGBITS, "no code section found");
    MOS_ASSERT_X(program_header->header_type == ELF_PH_T_LOAD, "no loadable segment found");

    // !! TODO: Implement this:
    // paging_handle_t init_pgd = { 0 };
    // mos_platform.mm_pgd_alloc(&init_pgd);
    // mos_platform.mm_pg_map_to_kvirt(init_pgd, program_header->seg_vaddr, (uintptr_t) initrd_data, pages, VM_NONE);

    // size_t pages = init_file->io.size / mos_platform.mm_page_size;
    // mos_platform.mm_pg_map_to_kvaddr(mos_platform.kernel_pg, program_header->seg_vaddr, 0, pages, VM_NONE);

    MOS_ASSERT_X(mos_platform.usermode_trampoline, "platform doesn't have a usermode trampoline");

    // the stack memory to be used if we enter the kernelmode by a trap / interrupt
    tss_entry.esp0 = (u32) kpage_alloc(1) + mos_platform.mm_page_size; // stack grows downwards from the top of the page

    uid_t root = { 0 };
    process_id_t pid1 = { 1 };
    thread_id_t tid1 = { 1 };

    process_id_t init_pid = create_process(pid1, root, (thread_entry_t) header->entry_point, (void *) 114514);
    thread_t *init_thread = get_thread(tid1);

    uintptr_t a = (uintptr_t) header + program_header->data_offset;

    mos_platform.mm_pg_map_to_kvaddr(mos_platform.kernel_pg, program_header->vaddr, a, 1, VM_USERMODE);

    MOS_ASSERT(init_pid.process_id == 1);
    MOS_ASSERT(init_thread->id.thread_id == 1);

    mos_platform.usermode_trampoline((uintptr_t) init_thread->stack.head, (uintptr_t) init_thread->entry_point, (uintptr_t) init_thread->arg);

    MOS_UNREACHABLE();
}

bool threads_foreach(const void *key, void *value)
{
    thread_id_t *tid = (thread_id_t *) key;
    thread_t *thread = (thread_t *) value;
    pr_info("thread %d", tid->thread_id);
    MOS_UNUSED(thread);

    // switch to the thread's context

    ;
    return false; // do not continue
}

void schedule()
{
    hashmap_foreach(thread_table, threads_foreach);
}
