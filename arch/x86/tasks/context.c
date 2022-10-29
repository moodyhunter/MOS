// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"

extern asmlinkage void x86_um_thread_startup();
extern asmlinkage void x86_context_switch_impl(uintptr_t *old_stack_ptr, uintptr_t new_stack, uintptr_t pgd);

void x86_setup_thread_context(thread_t *thread, thread_entry_t entry, void *arg)
{
    // x86_um_thread_startup needs [arg, entry_point] on the stack5
    uintptr_t entry_addr = (uintptr_t) entry;
    uintptr_t startup_func = (uintptr_t) x86_um_thread_startup;
    stack_push(&thread->stack, &entry_addr, sizeof(uintptr_t));
    stack_push(&thread->stack, &arg, sizeof(uintptr_t));
    stack_push(&thread->stack, &startup_func, sizeof(uintptr_t));
    stack_grow(&thread->stack, sizeof(reg_t) * 4); // space for esi, edi, ebx, ebp
}

void x86_context_switch(uintptr_t *old_stack, thread_t *to)
{
    // TODO: update TSS->esp0 on each context switch
    uintptr_t x = pg_page_get_mapped_paddr(x86_kpg_infra, to->owner->pagetable.ptr);
    x86_context_switch_impl(old_stack, (uintptr_t) to->stack.head, x);
}

void x86_context_switch_to_scheduler(uintptr_t *old_stack_ptr, uintptr_t new_stack)
{
    // pgd = 0 so that we don't switch to a different page table
    x86_context_switch_impl(old_stack_ptr, new_stack, 0);
}
