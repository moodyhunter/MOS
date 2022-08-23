// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/hashmap.h"
#include "mos/cmdline.h"
#include "mos/device/block.h"
#include "mos/filesystem/cpio/cpio.h"
#include "mos/filesystem/file.h"
#include "mos/filesystem/fs_fwd.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/path.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"
#include "mos/x86/tasks/tss_types.h"

extern void mos_test_engine_run_tests(); // defined in tests/test_engine.c

void schedule();
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

    mos_warn("V2Ray 4.45.2 started");

    process_init();
    thread_init();

    if (mos_cmdline_get_arg("mos_tests"))
    {
        mos_test_engine_run_tests();
    }

    blockdev_t *dev = blockdev_find("initrd");
    if (!dev)
        mos_panic("no initrd found");

    kmount(&root_path, &fs_cpio, dev);

    file_stat_t stat;
    if (!file_stat("/init", &stat))
        mos_panic("failed to stat /init");

    // print init info
    char perm[9];
    file_format_perm(stat.permissions, perm);
    pr_info("init: %zu bytes", stat.size);
    pr_info("      %s", perm);
    pr_info("      owner: %d, group: %d", stat.uid.uid, stat.gid.gid);

    void *init = kmalloc(stat.size);

    while (1)
        ;
#if 0
    // create the init process
    extern void main(void *arg);
    uintptr_t init_entry_addr = (uintptr_t) &main;

    MOS_ASSERT_X(mos_platform.usermode_trampoline, "platform doesn't have a usermode trampoline");

    process_id_t pid1 = { 1 };
    uid_t uid0 = { 0 };

    process_id_t init_pid = create_process(pid1, uid0, mos_platform.usermode_trampoline, (void *) init_entry_addr);
    MOS_ASSERT(init_pid.process_id == 1);

    thread_t *init_thread = get_thread((thread_id_t){ 1 });

    // !! FIXME
    tss_entry.esp0 = (u32) kpage_alloc(1) + mos_platform.mm_page_size;
    pr_warn("esp0: %#.8x", tss_entry.esp0);
    pr_warn("entry: %#.8x", (u32) init_entry_addr);
    pr_warn("stack: %#.8x", (u32) init_thread->stack.head);
    pr_warn("stack base: %#.8x", (u32) init_thread->stack.base);

    jump_to_usermain(init_entry_addr, (uintptr_t) init_thread->stack.head);

    // schedule
    schedule();
#endif
}

thread_t *context_switch(thread_t *cur, thread_t *next);

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
