// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/filesystem.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_type.h"
#include "mos/types.h"

#define PROCESS_MAGIC_PROC 0x50524F43 // "PROC"

typedef struct _hashmap hashmap_t;
typedef struct _terminal terminal_t;
extern hashmap_t *process_table;

void process_init();
void process_cleanup();

should_inline bool process_is_valid(process_t *process)
{
    return process != NULL && process->magic == PROCESS_MAGIC_PROC;
}

process_t *process_allocate(process_t *parent, uid_t euid, const char *name);
process_t *process_new(process_t *parent, uid_t effective_uid, const char *name, terminal_t *term, thread_entry_t entry, void *arg);
process_t *process_get(pid_t pid);

fd_t process_attach_ref_fd(process_t *process, io_t *file);
bool process_detach_fd(process_t *process, fd_t fd);

void process_attach_thread(process_t *process, thread_t *thread);
void process_attach_mmap(process_t *process, vmblock_t block, vm_type type, bool cow);

void process_handle_exit(process_t *process, int exit_code);
process_t *process_handle_fork(process_t *process);

void process_dump_mmaps(process_t *process);
