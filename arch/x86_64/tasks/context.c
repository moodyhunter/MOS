// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "mos/mm/paging/paging.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform_defs.h"
#include "mos/tasks/signal.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/descriptors/descriptors.h"

#include <elf.h>
#include <mos/lib/structures/stack.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
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
    signal_check_and_handle();
    x86_interrupt_return_impl(platform_thread_regs(current_thread));
}

static platform_regs_t *x86_setup_thread_common(thread_t *thread)
{
    thread->k_stack.head -= sizeof(platform_regs_t);
    platform_regs_t *regs = platform_thread_regs(thread);
    *regs = (platform_regs_t){ 0 };

    regs->cs = thread->mode == THREAD_MODE_KERNEL ? GDT_SEGMENT_KCODE : GDT_SEGMENT_USERCODE | 3;
    regs->ss = thread->mode == THREAD_MODE_KERNEL ? GDT_SEGMENT_KDATA : GDT_SEGMENT_USERDATA | 3;
    regs->sp = thread->mode == THREAD_MODE_KERNEL ? thread->k_stack.top : thread->u_stack.top;

    if (thread->mode == THREAD_MODE_USER)
        regs->eflags = 0x202 | (thread->owner->platform_options.iopl ? 0x3000 : 0);

    return regs;
}

static void x86_setup_main_thread(thread_t *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp)
{
    platform_regs_t *regs = x86_setup_thread_common(thread);
    regs->ip = entry;
    regs->di = argc;
    regs->si = argv;
    regs->dx = envp;
    regs->sp = sp;
}
__alias(x86_setup_main_thread, platform_context_setup_main_thread);

static void x86_setup_child_thread(thread_t *thread, thread_entry_t entry, void *arg)
{
    platform_regs_t *regs = x86_setup_thread_common(thread);
    regs->di = (ptr_t) arg;
    regs->ip = (ptr_t) entry;

    if (thread->mode == THREAD_MODE_KERNEL)
        return;

    MOS_ASSERT(thread->owner->mm == current_mm);
    MOS_ASSERT(thread != thread->owner->main_thread);

    regs->di = (ptr_t) arg;                      // argument
    stack_push_val(&thread->u_stack, (ptr_t) 0); // return address
    regs->sp = thread->u_stack.head;             // update the stack pointer
}
__alias(x86_setup_child_thread, platform_context_setup_child_thread);

static void x86_clone_forked_context(const thread_t *from, thread_t *to)
{
    platform_regs_t *to_regs = platform_thread_regs(to);
    *to_regs = *platform_thread_regs(from);
    to_regs->ax = 0; // return 0 for the child

    to->platform_options = from->platform_options; // TODO: COPY THE FPU STATE
    to->k_stack.head -= sizeof(platform_regs_t);
}
__alias(x86_clone_forked_context, platform_context_clone);

static void x86_switch_to_thread(ptr_t *scheduler_stack, const thread_t *to, switch_flags_t switch_flags)
{
    const switch_func_t switch_func = switch_flags & SWITCH_TO_NEW_USER_THREAD   ? x86_start_user_thread :
                                      switch_flags & SWITCH_TO_NEW_KERNEL_THREAD ? x86_start_kernel_thread :
                                                                                   x86_normal_switch_impl;

    x86_update_current_fsbase();
    per_cpu(x86_cpu_descriptor)->tss.rsp0 = to->k_stack.top;
    x86_context_switch_impl(scheduler_stack, to->k_stack.head, switch_func);
}
__alias(x86_switch_to_thread, platform_switch_to_thread);

static void x86_switch_to_scheduler(ptr_t *old_stack, ptr_t scheduler_stack)
{
    x86_context_switch_impl(old_stack, scheduler_stack, x86_normal_switch_impl);
}
__alias(x86_switch_to_scheduler, platform_switch_to_scheduler);

void x86_update_current_fsbase()
{
    const ptr_t fs_base = current_thread->platform_options.fs_base;

    if (cpu_has_feature(CPU_FEATURE_FSGSBASE))
    {
        if (once())
            x86_cpu_set_cr4(x86_cpu_get_cr4() | (1 << 16));

        __asm__ volatile("wrfsbase %0" ::"r"(fs_base) : "memory");
        return;
    }

    cpu_set_msr64(0xc0000100, fs_base); // IA32_FS_BASE
}


        return;

}
