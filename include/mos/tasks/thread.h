// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_type.h"
#include "mos/types.h"

typedef struct hashmap hashmap_t;
typedef void (*thread_entry_t)(void *arg); // also defined in task_type.h

extern hashmap_t *thread_table;

void thread_init();
void thread_deinit();

thread_t *create_thread(process_t *owner, thread_flags_t mode, thread_entry_t entry, void *arg);
thread_t *get_thread(tid_t id);
