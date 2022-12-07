// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/liballoc.h"
#include "mos/platform/platform.h"

#include <stdarg.h>

#define stdin  0
#define stdout 1
#define stderr 2

void printf(const char *fmt, ...);
void dprintf(int fd, const char *fmt, ...);
void dvprintf(int fd, const char *fmt, va_list ap);
void fatal_abort(const char *fmt, ...);

void start_thread(const char *name, thread_entry_t entry, void *arg);
