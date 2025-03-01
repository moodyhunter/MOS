// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/hashmap.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/types.h>

typedef struct _hashmap hashmap_t;

extern mos::HashMap<tid_t, Thread *> thread_table;

Thread *thread_allocate(Process *owner, thread_mode tflags);
void thread_destroy(Thread *thread);

PtrResult<Thread> thread_new(Process *owner, thread_mode mode, mos::string_view name, size_t stack_size, void *stack);
Thread *thread_complete_init(Thread *thread);
Thread *thread_get(tid_t id);
bool thread_wait_for_tid(tid_t tid);

[[noreturn]] void thread_exit(Thread *&&t);
[[noreturn]] void thread_exit_locked(Thread *&&t);
