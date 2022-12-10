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

#define SWITCH_MODE_NORMAL 0
#define SWITCH_MODE_IRET   1

extern asmlinkage void x86_context_switch_impl(uintptr_t *old_stack_ptr, uintptr_t new_stack, uintptr_t pgd, int switch_mode, void *switch_mode_arg);

void x86_setup_thread_context(thread_t *thread, downwards_stack_t *proxy_stack, thread_entry_t entry, void *arg)
{
    x86_thread_context_t *context = kmalloc(sizeof(x86_thread_context_t));
    context->inner.instruction = (uintptr_t) entry;
    context->ebp = 0;

    thread->context = &context->inner;
    // TODO: remove this hack
    stack_push(proxy_stack, &arg, sizeof(uintptr_t));
    uintptr_t zero = 0;
    stack_push(proxy_stack, &zero, sizeof(uintptr_t)); // a fake return address
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
    bool need_pgd_switch = current_cpu->pagetable.ptr != to->owner->pagetable.ptr;
    uintptr_t pgd_paddr = need_pgd_switch ? pg_page_get_mapped_paddr(x86_kpg_infra, to->owner->pagetable.ptr) : 0;
    tss_entry.esp0 = to->kernel_stack.top;
    x86_thread_context_t *context = container_of(to->context, x86_thread_context_t, inner);

    bool need_iret_switching = to->status == THREAD_STATUS_CREATED;

    mos_update_current(to); // this updates to->status to THREAD_STATUS_RUNNING

    if (unlikely(need_iret_switching))
        // eax is set to 0, which is exactly what a child process should get (from fork)
        x86_context_switch_impl(old_stack, to->stack.head, pgd_paddr, SWITCH_MODE_IRET, context);
    else
        x86_context_switch_impl(old_stack, to->stack.head, pgd_paddr, SWITCH_MODE_NORMAL, 0);
}

void x86_switch_to_scheduler(uintptr_t *old_stack_ptr, uintptr_t new_stack)
{
    // pgd = 0 so that we don't switch to a different page table
    // pass 0 as switch_mode so that we don't switch to IRET
    x86_context_switch_impl(old_stack_ptr, new_stack, 0, SWITCH_MODE_NORMAL, 0);
}

void x86_timer_handler()
{
    jump_to_scheduler();
}
