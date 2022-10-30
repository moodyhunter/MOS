// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/tasks/context.h"

#include "lib/structures/stack.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_type.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_interrupt.h"
#include "mos/x86/x86_platform.h"

#define SWITCH_MODE_NORMAL 0
#define SWITCH_MODE_IRET   1

typedef struct
{
    reg32_t ebp, eip;
} switch_stack_arg;

extern asmlinkage void x86_context_switch_impl(uintptr_t *old_stack_ptr, uintptr_t new_stack, uintptr_t pgd, int switch_mode, void *switch_mode_arg);

void x86_setup_thread_context(thread_t *thread, downwards_stack_t *proxy_stack, thread_entry_t entry, void *arg)
{
    MOS_UNUSED(entry);
    MOS_UNUSED(thread);
    stack_push(proxy_stack, &arg, sizeof(uintptr_t));
}

void x86_switch_to_thread(uintptr_t *old_stack, thread_t *to)
{
    uintptr_t pgd_paddr = pg_page_get_mapped_paddr(x86_kpg_infra, to->owner->pagetable.ptr);
    tss_entry.esp0 = to->kernel_stack.top;
    switch_stack_arg arg = { .ebp = (reg32_t) ((x86_stack_frame *) current_cpu->platform_context)->ebp, .eip = (reg32_t) to->current_instruction };

    bool need_iret_switching = to->status == THREAD_STATUS_FORKED || to->status == THREAD_STATUS_CREATED;

    mos_update_current(to); // this updates to->status to THREAD_STATUS_RUNNING

    if (unlikely(need_iret_switching))
        x86_context_switch_impl(old_stack, to->stack.head, pgd_paddr, SWITCH_MODE_IRET, (void *) &arg);
    else
        x86_context_switch_impl(old_stack, to->stack.head, pgd_paddr, 0, 0);
}

void x86_switch_to_scheduler(uintptr_t *old_stack_ptr, uintptr_t new_stack)
{
    // pgd = 0 so that we don't switch to a different page table
    // pass 0 as switch_mode so that we don't switch to IRET
    x86_context_switch_impl(old_stack_ptr, new_stack, 0, 0, 0);
}
