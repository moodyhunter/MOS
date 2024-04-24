// SPDX-License-Identifier: GPL-3.0-or-later

#include <dirent.h>
#include <mos/filesystem/fs_types.h>
#include <mos/tasks/signal_types.h>
#include <mos/types.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

fd_t open(const char *path, open_flags flags);

fd_t openat(fd_t fd, const char *path, open_flags flags);

int raise(signal_t sig);

int kill(pid_t pid, signal_t sig);

bool lstatat(int fd, const char *path, file_stat_t *buf);

#ifndef __NO_START_FUNCTION
void _start(size_t argc, char **argv, char **envp)
{
    MOS_UNUSED(envp);
    extern int main(int argc, char **argv);

// ensure that the stack is 16-byte aligned
#if defined(__x86_64__)
    __asm__ volatile("andq $-16, %rsp");
#else
#error "unsupported architecture"
#endif

    int r = main(argc, argv);
    syscall_exit(r);
}
#endif
