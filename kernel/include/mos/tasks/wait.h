// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_type.h"

wait_condition_t *wc_wait_for_thread(thread_t *thread);
void wc_condition_cleanup(wait_condition_t *condition);
