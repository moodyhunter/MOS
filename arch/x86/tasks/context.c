// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "lib/string.h"
#include "lib/structures/stack.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_types.h"
#include "mos/x86/descriptors/descriptor_types.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_interrupt.h"
#include "mos/x86/x86_platform.h"

typedef void (*switch_func_t)(void);

extern void x86_switch_impl_new_user_thread(void);
extern void x86_switch_impl_new_kernel_thread(void);
extern void x86_switch_impl_normal(void);

extern asmlinkage void x86_context_switch_impl(uintptr_t *old_stack, uintptr_t new_kstack, uintptr_t pgd, switch_func_t switcher, const x86_thread_context_t *context);

// called from assembly
asmlinkage uintptr_t x86_switch_impl_setup_user_thread(void)
{
    thread_t *current = current_thread;
    x86_thread_context_t *context = container_of(current->context, x86_thread_context_t, inner);
    const bool is_forked = context->is_forked;
    const bool is_main_thread = current == current->owner->threads[0];

    if (is_forked)
    {
        pr_info2("cpu %d: setting up forked thread (id: %ld) of process '%s' (%ld)", current_cpu->id, current->tid, current->owner->name, current->owner->pid);
        // do nothing
        goto done;
    }

    // for the main thread, we have to push all argv structures onto the stack
    // for any other threads, only a pointer specified by the user is pushed
    if (is_main_thread)
    {
        // set up the main thread of a 'new' process (not forked)
        pr_info2("cpu %d: setting up main thread (id: %ld) of process '%s' (%ld)", current_cpu->id, current->tid, current->owner->name, current->owner->pid);

        // the main thread of a process has no arg, because it uses argv
        MOS_ASSERT_X(context->arg == NULL, "arg should be NULL for the 'main' thread of process '%s' (%ld)", current->owner->name, current->owner->pid);

        const char *const *const src_argv = current->owner->argv.argv;
        const size_t argc = current->owner->argv.argc;

        const char *real_argv[argc + 1];
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
                strcpy((char *) real_argv[i], src_argv[i]);
            }
        }
        real_argv[argc] = NULL;

        // stack layout:
        // | argc | argv1 | argv2 | ... | argvN | NULL | arg0 | arg1 | ... | argN | NULL |

        stack_push(&current->u_stack, &real_argv, sizeof(char *) * (argc + 1)); // the argv vector

        const uintptr_t argv_ptr = current->u_stack.head; // stack_push changes head, so we need to save it
        stack_push(&current->u_stack, &argv_ptr, sizeof(uintptr_t));
        stack_push(&current->u_stack, &argc, sizeof(size_t));
    }
    else
    {
        stack_push(&current->u_stack, &context->arg, sizeof(void *));
    }

    const uintptr_t zero = 0;
    stack_push(&current->u_stack, &zero, sizeof(uintptr_t)); // return address

    context->inner.stack = current->u_stack.head; // update the stack pointer

done:
    return context->inner.stack;
}

void x86_setup_thread_context(thread_t *thread, thread_entry_t entry, void *arg)
{
    x86_process_options_t *options = thread->owner->platform_options;
    x86_thread_context_t *context = kzalloc(sizeof(x86_thread_context_t));
    context->inner.instruction = (uintptr_t) entry;
    context->inner.stack = thread->mode == THREAD_MODE_KERNEL ? thread->k_stack.head : thread->u_stack.head;
    context->arg = arg;
    context->is_forked = false;
    context->regs.iret_params.eflags = 0x202 | (options && options->iopl_enabled ? 0x3000 : 0);
    thread->context = &context->inner;
}

void x86_setup_forked_context(const thread_context_t *from, thread_context_t **to)
{
    const x86_thread_context_t *from_ctx = container_of(from, const x86_thread_context_t, inner);
    x86_thread_context_t *to_ctx = kzalloc(sizeof(x86_thread_context_t));
    *to = &to_ctx->inner;
    *to_ctx = *from_ctx; // copy everything
    to_ctx->is_forked = true;
}

void x86_switch_to_thread(uintptr_t *scheduler_stack, const thread_t *to, switch_flags_t switch_flags)
{
    per_cpu(x86_cpu_descriptor)->tss.esp0 = to->k_stack.top;
    const uintptr_t pgd_paddr = switch_flags & SWITCH_TO_NEW_PAGE_TABLE ? pg_get_mapped_paddr(x86_kpg_infra, to->owner->pagetable.pgd) : 0;
    const x86_thread_context_t *context = container_of(to->context, const x86_thread_context_t, inner);
    const switch_func_t switch_func = switch_flags & SWITCH_TO_NEW_USER_THREAD   ? x86_switch_impl_new_user_thread :
                                      switch_flags & SWITCH_TO_NEW_KERNEL_THREAD ? x86_switch_impl_new_kernel_thread :
                                                                                   x86_switch_impl_normal;
    x86_context_switch_impl(scheduler_stack, to->k_stack.head, pgd_paddr, switch_func, context);
}

void x86_switch_to_scheduler(uintptr_t *old_stack, uintptr_t scheduler_stack)
{
    // pgd = 0 so that we don't switch to a different page table
    x86_context_switch_impl(old_stack, scheduler_stack, 0, x86_switch_impl_normal, NULL);
}

void x86_timer_handler(u32 irq)
{
    MOS_UNUSED(irq);
    reschedule();
}
