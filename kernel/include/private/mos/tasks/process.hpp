// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/io/io.hpp"

#include <mos/hashmap.hpp>
#include <mos/tasks/task_types.hpp>
#include <optional>

typedef struct _hashmap hashmap_t;

/**
 * @brief A wrapper type for the standard I/O streams
 */
typedef struct
{
    io_t *in, *out, *err;
} stdio_t;

extern mos::HashMap<pid_t, Process *> ProcessTable;

const char *get_vmap_type_str(vmap_type_t type);

should_inline stdio_t current_stdio(void)
{
    return {
        .in = current_process->files[0].io,
        .out = current_process->files[1].io,
        .err = current_process->files[2].io,
    };
}

void process_destroy(Process *process);

Process *process_new(Process *parent, mos::string_view name, const stdio_t *ios);
std::optional<Process *> process_get(pid_t pid);

fd_t process_attach_ref_fd(Process *process, io_t *file, fd_flags_t flags);
io_t *process_get_fd(Process *process, fd_t fd);
bool process_detach_fd(Process *process, fd_t fd);

pid_t process_wait_for_pid(pid_t pid, u32 *exit_code, u32 flags);

[[noreturn]] void process_exit(Process *&&proc, u8 exit_code, signal_t signal);

void process_dump_mmaps(const Process *process);

bool process_register_signal_handler(Process *process, signal_t sig, const sigaction_t *sigaction);

Process *process_do_fork(Process *process);
long process_do_execveat(fd_t dirfd, const char *path, const char *const argv[], const char *const envp[], int flags);
