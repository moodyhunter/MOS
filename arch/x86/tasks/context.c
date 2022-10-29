// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "mos/tasks/task_type.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_interrupt.h"

extern asmlinkage void x86_um_thread_startup();
extern asmlinkage void x86_um_thread_post_fork();
extern asmlinkage void x86_context_switch_impl(uintptr_t *old_stack_ptr, uintptr_t new_stack, uintptr_t pgd, uintptr_t post_switch_func, uintptr_t post_switch_arg);

void x86_setup_thread_context(thread_t *thread, thread_entry_t entry, void *arg)
{
    // x86_um_thread_startup needs [arg, entry_point] on the stack5
    uintptr_t entry_addr = (uintptr_t) entry;
    stack_push(&thread->stack, &arg, sizeof(uintptr_t));
    stack_push(&thread->stack, &entry_addr, sizeof(uintptr_t));
}

void x86_switch_to_thread(uintptr_t *old_stack, thread_t *to)
{
    uintptr_t pgd_paddr = pg_page_get_mapped_paddr(x86_kpg_infra, to->owner->pagetable.ptr);
    uintptr_t post_switch_func = 0;
    uintptr_t post_switch_func_arg = 0;

    if (unlikely(to->status == THREAD_STATUS_CREATED))
        post_switch_func = (uintptr_t) x86_um_thread_startup;

    if (unlikely(to->status == THREAD_STATUS_FORKED))
    {
        post_switch_func = (uintptr_t) x86_um_thread_post_fork;
        post_switch_func_arg = to->current_instruction;
    }
    x86_enable_interrupts();
    to->status = THREAD_STATUS_RUNNING;
    x86_context_switch_impl(old_stack, (uintptr_t) to->stack.head, pgd_paddr, post_switch_func, post_switch_func_arg);
}

void x86_switch_to_scheduler(uintptr_t *old_stack_ptr, uintptr_t new_stack)
{
    // pgd = 0 so that we don't switch to a different page table
    x86_context_switch_impl(old_stack_ptr, new_stack, 0, 0, 0);
}
