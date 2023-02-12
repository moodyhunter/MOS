// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/liballoc.h"

#include <stdarg.h>

#define stdin  0
#define stdout 1
#define stderr 2

int __printf(1, 2) printf(const char *fmt, ...);
int __printf(2, 3) dprintf(int fd, const char *fmt, ...);
int vdprintf(int fd, const char *fmt, va_list ap);
void __printf(1, 2) fatal_abort(const char *fmt, ...);

tid_t start_thread(const char *name, thread_entry_t entry, void *arg);
