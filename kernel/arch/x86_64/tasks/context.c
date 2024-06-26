// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "mos/platform/platform_defs.h"
#include "mos/tasks/signal.h"
#include "mos/x86/descriptors/descriptors.h"
#include "mos/x86/tasks/fpu_context.h"

#include <mos/lib/structures/stack.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/types.h>
#include <mos/x86/cpu/cpu.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>
#include <mos_stdlib.h>
#include <mos_string.h>

typedef void (*switch_func_t)();

extern void x86_normal_switch_impl();
extern void x86_context_switch_impl(ptr_t *old_stack, ptr_t new_kstack, switch_func_t switcher);

static void x86_start_kernel_thread()
{
    platform_regs_t *regs = platform_thread_regs(current_thread);
    const thread_entry_t entry = (thread_entry_t) regs->ip;
    void *const arg = (void *) regs->di;
    entry(arg);
    MOS_UNREACHABLE();
}

static void x86_start_user_thread()
{
    platform_regs_t *regs = platform_thread_regs(current_thread);
    signal_exit_to_user_prepare(regs);
    x86_interrupt_return_impl(regs);
}

static platform_regs_t *x86_setup_thread_common(thread_t *thread)
{
    MOS_ASSERT_X(thread->platform_options.xsaveptr == NULL, "xsaveptr should be NULL");
    thread->platform_options.xsaveptr = kmalloc(xsave_area_slab);
    thread->k_stack.head -= sizeof(platform_regs_t);
    platform_regs_t *regs = platform_thread_regs(thread);
    *regs = (platform_regs_t){ 0 };

    regs->cs = thread->mode == THREAD_MODE_KERNEL ? GDT_SEGMENT_KCODE : GDT_SEGMENT_USERCODE | 3;
    regs->ss = thread->mode == THREAD_MODE_KERNEL ? GDT_SEGMENT_KDATA : GDT_SEGMENT_USERDATA | 3;
    regs->sp = thread->mode == THREAD_MODE_KERNEL ? thread->k_stack.top : thread->u_stack.top;

    if (thread->mode == THREAD_MODE_USER)
    {
        regs->eflags = 0x202;
        if (thread->owner->platform_options.iopl)
            regs->eflags |= 0x3000;
    }

    return regs;
}

void platform_context_setup_main_thread(thread_t *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp)
{
    platform_regs_t *regs = x86_setup_thread_common(thread);
    regs->ip = entry;
    regs->di = argc;
    regs->si = argv;
    regs->dx = envp;
    regs->sp = sp;
}

void platform_context_cleanup(thread_t *thread)
{
    if (thread->mode == THREAD_MODE_USER)
        if (thread->platform_options.xsaveptr)
            kfree(thread->platform_options.xsaveptr), thread->platform_options.xsaveptr = NULL;
}

void platform_context_setup_child_thread(thread_t *thread, thread_entry_t entry, void *arg)
{
    platform_regs_t *regs = x86_setup_thread_common(thread);
    regs->di = (ptr_t) arg;
    regs->ip = (ptr_t) entry;

    if (thread->mode == THREAD_MODE_KERNEL)
        return;

    MOS_ASSERT(thread->owner->mm == current_mm);
    MOS_ASSERT(thread != thread->owner->main_thread);

    regs->di = (ptr_t) arg;          // argument
    regs->sp = thread->u_stack.head; // update the stack pointer
}

void platform_context_clone(const thread_t *from, thread_t *to)
{
    platform_regs_t *to_regs = platform_thread_regs(to);
    *to_regs = *platform_thread_regs(from);
    to_regs->ax = 0; // return 0 for the child

    // synchronise the sp of user stack
    if (to->mode == THREAD_MODE_USER)
    {
        to->u_stack.head = to_regs->sp;
        to->platform_options.xsaveptr = kmalloc(xsave_area_slab);
        memcpy(to->platform_options.xsaveptr, from->platform_options.xsaveptr, platform_info->arch_info.xsave_size);
    }

    to->platform_options.fs_base = from->platform_options.fs_base;
    to->platform_options.gs_base = from->platform_options.gs_base;
    to->k_stack.head -= sizeof(platform_regs_t);
}

void platform_switch_to_thread(ptr_t *scheduler_stack, thread_t *new_thread, switch_flags_t switch_flags)
{
    thread_t *const old_thread = current_thread;
    const switch_func_t switch_func = switch_flags & SWITCH_TO_NEW_USER_THREAD   ? x86_start_user_thread :
                                      switch_flags & SWITCH_TO_NEW_KERNEL_THREAD ? x86_start_kernel_thread :
                                                                                   x86_normal_switch_impl;

    x86_xsave_thread(old_thread);
    x86_xrstor_thread(new_thread);
    x86_set_fsbase(new_thread);

    __atomic_store_n(&current_cpu->thread, new_thread, __ATOMIC_SEQ_CST);
    __atomic_store_n(&per_cpu(x86_cpu_descriptor)->tss.rsp0, new_thread->k_stack.top, __ATOMIC_SEQ_CST);

    x86_context_switch_impl(scheduler_stack, new_thread->k_stack.head, switch_func);
}

void platform_switch_to_scheduler(ptr_t *old_stack, ptr_t scheduler_stack)
{
    x86_context_switch_impl(old_stack, scheduler_stack, x86_normal_switch_impl);
}

void x86_set_fsbase(thread_t *thread)
{
    __asm__ volatile("wrfsbase %0" ::"r"(thread->platform_options.fs_base) : "memory");
}
