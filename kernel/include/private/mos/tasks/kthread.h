// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"

void kthread_init(void);
thread_t *kthread_create(thread_entry_t entry, void *arg, const char *name);
