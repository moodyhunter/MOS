// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/stdio.h"
#include "mos/platform/platform.h"
#include "mos/syscall/usermode.h"

#define stdin  0
#define stdout 1
#define stderr 2

void printf(const char *fmt, ...);

void start_thread(const char *name, thread_entry_t entry, void *arg);
