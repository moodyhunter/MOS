// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "mos/mm/paging/paging.h"
#include "mos/platform/platform_defs.h"
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

typedef void (*switch_func_t)(x86_thread_context_t *context);

extern void x86_normal_switch_impl(x86_thread_context_t *context);
extern void x86_context_switch_impl(x86_thread_context_t *context, ptr_t *old_stack, ptr_t new_kstack, switch_func_t switcher);

static void x86_start_kernel_thread(x86_thread_context_t *ctx)
{
    thread_entry_t entry = (thread_entry_t) ctx->regs.ip;
    void *arg = (void *) ctx->regs.di;
    entry(arg);
    MOS_UNREACHABLE();
}

static void x86_start_user_thread(x86_thread_context_t *context)
{
    x86_interrupt_return_impl(&context->regs);
}

should_inline x86_thread_context_t *x86_setup_thread_common(thread_t *thread)
{
    if (likely(thread->context))
        return thread->context;
    x86_thread_context_t *context = thread->context = kmalloc(sizeof(x86_thread_context_t));

    context->regs.cs = GDT_SEGMENT_USERCODE | 3;
    context->regs.ss = GDT_SEGMENT_USERDATA | 3;
    context->regs.sp = thread->mode == THREAD_MODE_KERNEL ? thread->k_stack.top : thread->u_stack.top;

    if (thread->mode == THREAD_MODE_USER)
    {
        x86_process_options_t *options = thread->owner->platform_options;
        context->regs.eflags = 0x202 | (options && options->iopl_enabled ? 0x3000 : 0);
    }

    return context;
}

static void x86_setup_main_thread(thread_t *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp)
{
    x86_thread_context_t *context = x86_setup_thread_common(thread);
    context->regs.ip = entry;
    context->regs.di = argc;
    context->regs.si = argv;
    context->regs.dx = envp;
    context->regs.sp = sp;
}
__alias(x86_setup_main_thread, platform_context_setup_main_thread);

static void x86_setup_child_thread(thread_t *thread, thread_entry_t entry, void *arg)
{
    MOS_ASSERT_X(!thread->context, "thread %pt already has a context", (void *) thread);
    x86_thread_context_t *context = x86_setup_thread_common(thread);
    context->regs.di = (ptr_t) arg;
    context->regs.ip = (ptr_t) entry;

    if (thread->mode == THREAD_MODE_KERNEL)
        return;

    x86_process_options_t *options = thread->owner->platform_options;
    context->regs.eflags = 0x202 | (options && options->iopl_enabled ? 0x3000 : 0);

    MOS_ASSERT(thread->owner->mm == current_mm);
    MOS_ASSERT(thread != thread->owner->main_thread);

    context->regs.di = (ptr_t) arg;              // argument
    stack_push_val(&thread->u_stack, (ptr_t) 0); // return address
    context->regs.sp = thread->u_stack.head;     // update the stack pointer
}
__alias(x86_setup_child_thread, platform_context_setup_child_thread);

static void x86_clone_forked_context(const void *from, void **to)
{
    const x86_thread_context_t *from_ctx = from;
    x86_thread_context_t *to_ctx = kmalloc(sizeof(x86_thread_context_t));
    *to = to_ctx;
    *to_ctx = *from_ctx; // copy everything
    to_ctx->regs.ax = 0; // return 0 for the child
}
__alias(x86_clone_forked_context, platform_context_clone);

static void x86_switch_to_thread(ptr_t *scheduler_stack, const thread_t *to, switch_flags_t switch_flags)
{
    per_cpu(x86_cpu_descriptor)->tss.rsp0 = to->k_stack.top;
    const switch_func_t switch_func = switch_flags & SWITCH_TO_NEW_USER_THREAD   ? x86_start_user_thread :
                                      switch_flags & SWITCH_TO_NEW_KERNEL_THREAD ? x86_start_kernel_thread :
                                                                                   x86_normal_switch_impl;

    x86_thread_context_t context = *(x86_thread_context_t *) to->context; // make a copy
    x86_context_switch_impl(&context, scheduler_stack, to->k_stack.head, switch_func);
}
__alias(x86_switch_to_thread, platform_switch_to_thread);

static void x86_switch_to_scheduler(ptr_t *old_stack, ptr_t scheduler_stack)
{
    x86_context_switch_impl(NULL, old_stack, scheduler_stack, x86_normal_switch_impl);
}
__alias(x86_switch_to_scheduler, platform_switch_to_scheduler);

void x86_jump_to_userspace()
{
    x86_update_current_fsbase();
    x86_interrupt_return_impl(&((x86_thread_context_t *) current_thread->context)->regs);
}

static bool has_rdwrfsgsbase()
{
    static bool result = false;
    if (once())
    {
        uintn id = x86_cpuid(b, leaf = 7, c = 0);
        result = GET_BIT(id, 0);

        // enable wrfsbase (if supported)
        if (result)
            x86_cpu_set_cr4(x86_cpu_get_cr4() | (1 << 16));
    }

    return result;
}

void x86_update_current_fsbase()
{
    x86_thread_context_t *ctx = current_thread->context;
    const ptr_t fs_base = ctx->fs_base;

    if (!has_rdwrfsgsbase())
    {
        cpu_set_msr64(0xc0000100, fs_base); // IA32_FS_BASE
        return;
    }

    __asm__ volatile("wrfsbase %0" ::"r"(fs_base) : "memory");
}
