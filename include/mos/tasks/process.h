// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_type.h"
#include "mos/types.h"

void process_init();
void process_deinit();

process_t *allocate_process(process_id_t parent_pid, uid_t effective_uid, thread_entry_t entry, void *arg);
process_t *get_process(process_id_t pid);
