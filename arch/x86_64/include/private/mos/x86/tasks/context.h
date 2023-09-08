// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/x86_platform.h"

typedef struct
{
    x86_stack_frame regs;
    ptr_t fs_base, gs_base;
} __packed x86_thread_context_t;

typedef struct
{
    bool iopl_enabled;
} x86_process_options_t;

noreturn void x86_jump_to_userspace(void);

void x86_update_current_fsbase();
