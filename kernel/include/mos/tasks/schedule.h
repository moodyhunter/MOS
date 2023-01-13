// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"

void unblock_scheduler(void);
noreturn void scheduler(void);

void reschedule_for_wait_condition(wait_condition_t *wait_condition);
void reschedule(void);
