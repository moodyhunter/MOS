// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/task_types.h>
#include <mos/types.h>
#include <mos/x86/x86_platform.h>

typedef struct
{
    x86_stack_frame regs;
    void *arg;
    bool is_forked; // true if this context is a forked copy of another context
} __packed x86_thread_context_t;

typedef struct
{
    bool iopl_enabled;
} x86_process_options_t;

void x86_setup_thread_context(thread_t *thread, thread_entry_t entry, void *arg);
void x86_setup_forked_context(const void *from, void **to);

void x86_switch_to_thread(ptr_t *old_stack, const thread_t *to, switch_flags_t switch_flags);
void x86_switch_to_scheduler(ptr_t *old_stack, ptr_t new_stack);

void x86_timer_handler(u32 irq);
