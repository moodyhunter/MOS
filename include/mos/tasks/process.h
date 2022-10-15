// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/filesystem.h"
#include "mos/mos_global.h"
#include "mos/tasks/task_type.h"
#include "mos/types.h"

void process_init();
void process_deinit();

should_inline bool process_is_valid(process_t *process)
{
    return process != NULL && process->magic[0] == 'P' && process->magic[1] == 'R' && process->magic[2] == 'O' && process->magic[3] == 'C';
}

process_t *allocate_process(process_id_t parent_pid, uid_t effective_uid, thread_entry_t entry, void *arg);
process_t *get_process(process_id_t pid);
fd_t process_add_fd(process_t *process, io_t *file);
bool process_remove_fd(process_t *process, fd_t fd);
