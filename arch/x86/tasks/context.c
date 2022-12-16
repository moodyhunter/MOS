// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "lib/string.h"
#include "lib/structures/stack.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_types.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_interrupt.h"
#include "mos/x86/x86_platform.h"

extern asmlinkage void x86_context_switch_impl(uintptr_t *old_stack_ptr, uintptr_t new_stack, uintptr_t pgd, bool iret_switching, const void *switch_mode_arg);

void x86_setup_thread_context(thread_t *thread, downwards_stack_t *proxy_stack, thread_entry_t entry, void *arg)
{
    x86_thread_context_t *context = kmalloc(sizeof(x86_thread_context_t));
    context->inner.instruction = (uintptr_t) entry;
    context->ebp = 0;
    thread->context = &context->inner;

    const uintptr_t zero = 0;
    stack_push(proxy_stack, &arg, sizeof(uintptr_t));
    stack_push(proxy_stack, &zero, sizeof(uintptr_t)); // a fake return address

    if (thread->mode == THREAD_MODE_KERNEL)
    {
        // pop, pop, pop, pop, ret sequence
        stack_push(proxy_stack, &entry, sizeof(uintptr_t));
        // esi, edi, ebx, ebp
        stack_push(proxy_stack, &zero /* esi */, sizeof(uintptr_t));
        stack_push(proxy_stack, &zero /* edi */, sizeof(uintptr_t));
        stack_push(proxy_stack, &zero /* ebx */, sizeof(uintptr_t));
        stack_push(proxy_stack, &zero /* ebp */, sizeof(uintptr_t));
    }
}

void x86_copy_thread_context(platform_context_t *from, platform_context_t **to)
{
    x86_thread_context_t *from_arg = container_of(from, x86_thread_context_t, inner);
    x86_thread_context_t *to_arg = kmalloc(sizeof(x86_thread_context_t));
    memcpy(to_arg, from_arg, sizeof(x86_thread_context_t));
    *to = &to_arg->inner;
}

void x86_switch_to_thread(uintptr_t *old_stack, thread_t *to)
{
    const bool need_pgd_switch = current_cpu->pagetable.ptr != to->owner->pagetable.ptr;
    const bool need_iret_switching = to->mode == THREAD_MODE_USER && to->state == THREAD_STATE_CREATED;

    const uintptr_t pgd_paddr = need_pgd_switch ? pg_page_get_mapped_paddr(x86_kpg_infra, to->owner->pagetable.ptr) : 0;
    const downwards_stack_t *stack = to->mode == THREAD_MODE_KERNEL ? &to->kernel_stack : &to->stack;

    per_cpu(x86_tss.tss)->esp0 = to->kernel_stack.top;

    mos_update_current(to); // this updates to->mode to THREAD_MODE_RUNNING

    // eax will be set to 0 (when need_iret_switching), which is exactly what a child process should get (from fork)
    const x86_thread_context_t *context = container_of(to->context, x86_thread_context_t, inner);
    x86_context_switch_impl(old_stack, stack->head, pgd_paddr, need_iret_switching, context);
}

void x86_switch_to_scheduler(uintptr_t *old_stack_ptr, uintptr_t new_stack)
{
    // pgd = 0 so that we don't switch to a different page table
    x86_context_switch_impl(old_stack_ptr, new_stack, 0, false, 0);
}

void x86_timer_handler()
{
    reschedule();
}
