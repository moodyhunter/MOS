// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/platform/platform_defs.h"
#include "mos/x86/descriptors/descriptors.h"

#include <mos/lib/structures/stack.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/x86/cpu/cpu.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/tasks/context.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>
#include <stdlib.h>
#include <string.h>

typedef void (*switch_func_t)(void);

extern void x86_switch_impl_new_user_thread(void);
extern void x86_switch_impl_new_kernel_thread(void);
extern void x86_switch_impl_normal(void);

extern asmlinkage void x86_context_switch_impl(ptr_t *old_stack, ptr_t new_kstack, ptr_t pgd, switch_func_t switcher, const x86_thread_context_t *context);

// called from assembly
void x86_switch_impl_setup_user_thread(x86_thread_context_t *context)
{
    thread_t *current = current_thread;
    const bool is_forked = context->is_forked;
    const bool is_main_thread = current == current->owner->threads[0];

    if (is_forked)
    {
        mos_debug(scheduler, "cpu %d: setting up forked thread %pt of process %pp", current_cpu->id, (void *) current, (void *) current->owner);
        return;
    }

    if (is_main_thread)
    {
        // for the main thread, we have to push all argv structures onto the stack
        // for any other threads, only a pointer specified by the user is passed by register RDI
        // set up the main thread of a 'new' process (not forked)
        mos_debug(scheduler, "cpu %d: setting up main thread %pt of process %pp", current_cpu->id, (void *) current, (void *) current->owner);

        // the main thread of a process has no arg, because it uses argv
        MOS_ASSERT_X(context->arg == NULL, "arg should be NULL for the 'main' thread of process %pp", (void *) current->owner);

        const char *const *const src_argv = current->owner->argv.argv;
        const size_t argc = current->owner->argv.argc;

        char *real_argv[argc + 1];
        for (size_t i = 0; i < argc; i++)
        {
            if (src_argv[i] == NULL)
            {
                pr_warn("argv[%zu] is NULL, replacing with NULL", i);
                real_argv[i] = NULL;
            }
            else
            {
                real_argv[i] = stack_grow(&current->u_stack, strlen(src_argv[i]) + 1);
                strcpy(real_argv[i], src_argv[i]);
            }
        }
        real_argv[argc] = NULL;

        // stack layout:
        // | argv1 | argv2 | ... | argvN | NULL | arg0 | arg1 | ... | argN | NULL |
        stack_push(&current->u_stack, &real_argv, sizeof(char *) * (argc + 1)); // the argv vector

        const ptr_t argv_ptr = current->u_stack.head; // stack_push changes head, so we need to save it

        context->regs.di = argc;
        context->regs.si = argv_ptr;
        context->regs.dx = 0; // TODO: envp
    }
    else
    {
        context->regs.di = (ptr_t) context->arg;
    }

    const ptr_t zero = 0;
    stack_push(&current->u_stack, &zero, sizeof(ptr_t)); // return address

    context->regs.iret_params.sp = current->u_stack.head; // update the stack pointer
}

void x86_setup_thread_context(thread_t *thread, thread_entry_t entry, void *arg)
{
    x86_process_options_t *options = thread->owner->platform_options;
    x86_thread_context_t *context = kzalloc(sizeof(x86_thread_context_t));
    context->arg = arg;
    context->is_forked = false;
    context->regs.iret_params.ip = (ptr_t) entry;
    context->regs.iret_params.sp = thread->mode == THREAD_MODE_KERNEL ? thread->k_stack.top : thread->u_stack.top;
    context->regs.iret_params.eflags = 0x202 | (options && options->iopl_enabled ? 0x3000 : 0);
    thread->context = context;
}

void x86_setup_forked_context(const void *from, void **to)
{
    const x86_thread_context_t *from_ctx = from;
    x86_thread_context_t *to_ctx = kzalloc(sizeof(x86_thread_context_t));
    *to = to_ctx;
    *to_ctx = *from_ctx; // copy everything
    to_ctx->is_forked = true;
}

void x86_switch_to_thread(ptr_t *scheduler_stack, const thread_t *to, switch_flags_t switch_flags)
{
    per_cpu(x86_cpu_descriptor)->tss.rsp0 = to->k_stack.top;
    const ptr_t pgd_paddr = pgd_pfn(to->owner->mm->pgd) * MOS_PAGE_SIZE;
    const switch_func_t switch_func = switch_flags & SWITCH_TO_NEW_USER_THREAD   ? x86_switch_impl_new_user_thread :
                                      switch_flags & SWITCH_TO_NEW_KERNEL_THREAD ? x86_switch_impl_new_kernel_thread :
                                                                                   x86_switch_impl_normal;

    const x86_thread_context_t context = *(x86_thread_context_t *) to->context;
    x86_context_switch_impl(scheduler_stack, to->k_stack.head, pgd_paddr, switch_func, &context);
}

void x86_switch_to_scheduler(ptr_t *old_stack, ptr_t scheduler_stack)
{
    // pgd = 0 so that we don't switch to a different page table
    x86_context_switch_impl(old_stack, scheduler_stack, 0, x86_switch_impl_normal, NULL);
}

void x86_timer_handler(u32 irq)
{
    MOS_UNUSED(irq);
    reschedule();
}
