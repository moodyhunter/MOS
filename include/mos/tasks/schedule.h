// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/platform/platform.h"

void mos_update_current(thread_t *thread);
noreturn void scheduler(void);
void jump_to_scheduler(void);
