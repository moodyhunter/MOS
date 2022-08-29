// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/filesystem.h"
#include "mos/macro_magic.h"
#include "mos/mos_global.h"
#include "mos/types.h"

#define SYSCALL_DO_TYPEDEF(name, ret, ...) typedef ret (*CONCAT(CONCAT(name##_syscall, NUM_OF_ARGS(__VA_ARGS__)), _t))(__VA_ARGS__);
#define SYSCALL_DO_DECL(name, ret, ...)    ret CONCAT(_syscall, _##name)(__VA_ARGS__);
#define SYSCALL_FUNC_NAME(name, ret, ...)  CONCAT(_syscall, _##name)
#define SYSCALL_EXPAND_N(arg, N)           MOS_SYSCALL_##N(arg)

#define MOS_SYSCALL_DEFINE(ret, name) ret CONCAT(MOS_SYSCALL_, SYSCALL_##name)(SYSCALL_FUNC_NAME)

#define MOS_SYSCALL_FOREACH(X)                                                                                                                  \
    MOS_WARNING_PUSH                                                                                                                            \
    MOS_WARNING_DISABLE("-Wpedantic")                                                                                                           \
    FOR_EACH_1ARG(SYSCALL_EXPAND_N, X, INT_SEQ(ID, EXPAND(MOS_SYSCALL_MAX)))                                                                    \
    MOS_WARNING_POP

#define SYSCALL_file_open 1
#define MOS_SYSCALL_1(X)  X(file_open, fd_t, const char *file, file_open_flags flags)

#define SYSCALL_file_stat 2
#define MOS_SYSCALL_2(X)  X(file_stat, bool, const char *file, file_stat_t *out)

#define SYSCALL_io_read  3
#define MOS_SYSCALL_3(X) X(io_read, size_t, fd_t fd, void *buf, size_t size, size_t offset)

#define SYSCALL_io_write 4
#define MOS_SYSCALL_4(X) X(io_write, size_t, fd_t fd, const void *buf, size_t size, size_t offset)

#define SYSCALL_io_close 5
#define MOS_SYSCALL_5(X) X(io_close, bool, fd_t fd)

#define SYSCALL_panic    6
#define MOS_SYSCALL_6(X) X(panic, int)

#define MOS_SYSCALL_MAX 6

MOS_SYSCALL_FOREACH(SYSCALL_DO_DECL)
