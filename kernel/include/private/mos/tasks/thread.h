// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/task_types.h>

#define THREAD_MAGIC_THRD MOS_FOURCC('T', 'H', 'R', 'D')

typedef struct _hashmap hashmap_t;

extern hashmap_t *thread_table;

should_inline bool thread_is_valid(thread_t *thread)
{
    return thread && thread->magic == THREAD_MAGIC_THRD;
}

thread_t *thread_allocate(process_t *owner, thread_mode tflags);
thread_t *thread_new(process_t *owner, thread_mode mode, const char *name, thread_entry_t entry, void *arg);
thread_t *thread_setup_complete(thread_t *thread);
thread_t *thread_get(tid_t id);
bool thread_wait_for_tid(tid_t tid);

void thread_handle_exit(thread_t *t);
