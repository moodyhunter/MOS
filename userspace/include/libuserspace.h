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

void start_thread(const char *name, thread_entry_t entry, void *arg);

// clang-format off
should_inline __malloc void *malloc(size_t size) { return liballoc_malloc(size); }
should_inline void free(void *ptr) { liballoc_free(ptr); }
should_inline __malloc void *calloc(size_t nmemb, size_t size) { return liballoc_calloc(nmemb, size); }
should_inline __malloc void *realloc(void *ptr, size_t size) { return liballoc_realloc(ptr, size); }
// clang-format on
