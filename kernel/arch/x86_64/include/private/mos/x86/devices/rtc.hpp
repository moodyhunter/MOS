// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.hpp"

#include <mos/types.hpp>

void rtc_init();
bool rtc_irq_handler(u32 irq, void *data);
void rtc_read_time(timeval_t *time);
