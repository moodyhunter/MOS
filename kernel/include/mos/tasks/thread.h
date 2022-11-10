// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/tasks/task_type.h"
#include "mos/types.h"

typedef struct hashmap hashmap_t;
typedef void (*thread_entry_t)(void *arg); // also defined in task_type.h

extern hashmap_t *thread_table;

should_inline bool thread_is_valid(thread_t *thread)
{
    return thread && thread->magic[0] == 'T' && thread->magic[1] == 'H' && thread->magic[2] == 'R' && thread->magic[3] == 'D';
}

void thread_init();
void thread_deinit();

thread_t *thread_allocate(process_t *owner, thread_flags_t tflags);
thread_t *thread_new(process_t *owner, thread_flags_t mode, thread_entry_t entry, void *arg);
thread_t *thread_get(tid_t id);

void thread_handle_exit(thread_t *t);
