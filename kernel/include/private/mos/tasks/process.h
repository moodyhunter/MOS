// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/task_types.h>

#define PROCESS_MAGIC_PROC MOS_FOURCC('P', 'R', 'O', 'C')

typedef struct _hashmap hashmap_t;
typedef struct _terminal terminal_t;
extern hashmap_t *process_table;

void process_init(void);
void process_deinit(void);

should_inline bool process_is_valid(process_t *process)
{
    return process != NULL && process->magic == PROCESS_MAGIC_PROC;
}

process_t *process_allocate(process_t *parent, const char *name);
process_t *process_new(process_t *parent, const char *name, terminal_t *term, thread_entry_t entry, argv_t argv);
process_t *process_get(pid_t pid);

fd_t process_attach_ref_fd(process_t *process, io_t *file);
io_t *process_get_fd(process_t *process, fd_t fd);
bool process_detach_fd(process_t *process, fd_t fd);

void process_attach_thread(process_t *process, thread_t *thread);
void process_attach_mmap(process_t *process, vmblock_t block, vmap_content_t type, vmap_flags_t flags);
void process_detach_mmap(process_t *process, vmblock_t block);
uintptr_t process_grow_heap(process_t *process, size_t npages);

bool process_wait_for_pid(pid_t pid);

void process_handle_exit(process_t *process, int exit_code);
process_t *process_handle_fork(process_t *process);

void process_dump_mmaps(const process_t *process);
