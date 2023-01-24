// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_types.h"
#include "mos/types.h"
#include "mos/x86/x86_platform.h"

typedef struct
{
    platform_context_t inner;
    x86_stack_frame regs;
    void *arg;
} __packed x86_thread_context_t;

void x86_setup_thread_context(thread_t *thread, thread_entry_t entry, void *arg);
void x86_copy_thread_context(platform_context_t *from, platform_context_t **to);

void x86_switch_to_thread(uintptr_t *old_stack, const thread_t *to, switch_flags_t switch_flags);
void x86_switch_to_scheduler(uintptr_t *old_stack, uintptr_t new_stack);

void x86_timer_handler(u32 irq);
