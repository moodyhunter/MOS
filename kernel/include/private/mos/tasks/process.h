// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.h"

#include <mos/tasks/task_types.h>

#define PROCESS_MAGIC_PROC MOS_FOURCC('P', 'R', 'O', 'C')

typedef struct _hashmap hashmap_t;

extern hashmap_t process_table;

should_inline bool process_is_valid(process_t *process)
{
    return process != NULL && process->magic == PROCESS_MAGIC_PROC;
}

should_inline stdio_t current_stdio(void)
{
    return (stdio_t){
        .in = current_process->files[0],
        .out = current_process->files[1],
        .err = current_process->files[2],
    };
}

process_t *process_allocate(process_t *parent, const char *name);
void process_destroy(process_t *process);

process_t *process_new(process_t *parent, const char *name, const stdio_t *ios);
process_t *process_get(pid_t pid);

fd_t process_attach_ref_fd(process_t *process, io_t *file);
io_t *process_get_fd(process_t *process, fd_t fd);
bool process_detach_fd(process_t *process, fd_t fd);

ptr_t process_grow_heap(process_t *process, size_t npages);

pid_t process_wait_for_pid(pid_t pid, u32 *exit_code, u32 flags);

noreturn void process_handle_exit(process_t *process, u8 exit_code, signal_t signal);

void process_dump_mmaps(const process_t *process);

bool process_register_signal_handler(process_t *process, signal_t sig, const sigaction_t *sigaction);

process_t *process_do_fork(process_t *process);
long process_do_execveat(process_t *process, fd_t dirfd, const char *path, const char *const argv[], const char *const envp[], int flags);
