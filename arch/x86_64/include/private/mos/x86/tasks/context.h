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

void x86_timer_handler(u32 irq);

noreturn void x86_jump_to_userspace(x86_stack_frame *context);
