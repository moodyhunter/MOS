// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

#include <stdarg.h>

#ifdef __cplusplus
#define __mosapi extern "C"
#else
#define __mosapi
#endif

#define stdin  0
#define stdout 1
#define stderr 2

__mosapi int __printf(1, 2) printf(const char *fmt, ...);
__mosapi int __printf(2, 3) dprintf(int fd, const char *fmt, ...);
__mosapi int vdprintf(int fd, const char *fmt, va_list ap);
__mosapi void __printf(1, 2) fatal_abort(const char *fmt, ...);

__mosapi tid_t start_thread(const char *name, thread_entry_t entry, void *arg);
