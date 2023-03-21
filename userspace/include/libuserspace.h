// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/lib.h>
#include <mos/mos_global.h>
#include <mos/types.h>
#include <stdarg.h>

MOSAPI int __printf(1, 2) printf(const char *fmt, ...);
MOSAPI int __printf(2, 3) dprintf(int fd, const char *fmt, ...);
MOSAPI int vdprintf(int fd, const char *fmt, va_list ap);
MOSAPI void __printf(1, 2) fatal_abort(const char *fmt, ...);

MOSAPI tid_t start_thread(const char *name, thread_entry_t entry, void *arg);

MOSAPI int atexit(void (*func)(void));
