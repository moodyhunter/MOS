// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_type.h"
#include "mos/types.h"

void x86_setup_thread_context(thread_t *thread, thread_entry_t entry, void *arg);

void x86_context_switch(uintptr_t *old_stack, thread_t *to);
void x86_context_switch_to_scheduler(uintptr_t *old_stack, uintptr_t new_stack);
