// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_types.h"
#include "mos/types.h"

typedef struct
{
    platform_context_t inner;
    reg32_t ebp;
    void *arg;
} __packed x86_thread_context_t;

void x86_setup_thread_context(thread_t *thread, thread_entry_t entry, void *arg);
void x86_copy_thread_context(platform_context_t *from, platform_context_t **to);

void x86_switch_to_thread(uintptr_t *old_stack, thread_t *to);
void x86_switch_to_scheduler(uintptr_t *old_stack, uintptr_t new_stack);

void x86_timer_handler();
