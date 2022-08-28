// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/tasks/task_type.h"
#include "mos/types.h"

typedef struct
{
    as_context_t;
    reg_t ebp; // frame pointer
    reg_t esi; // source index
    reg_t edi; // destination index
    reg_t ebx;
    reg_t cs, ds, es, fs, gs, ss;
} x86_context_t;

void x86_thread_context_init(thread_t *t, thread_entry_t entry, void *arg);
void x86_context_switch(thread_t *from, thread_t *to);
void x86_context_jump(thread_t *t);
