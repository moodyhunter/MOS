// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#define MOS_TEST_LOG mos_test_engine_log

#include "mos/drivers/screen.h"
#include "mos/kernel.h"
#include "mos/mos_global.h"
#include "mos/stdio.h"
#include "tinytest.h"

void mos_test_engine_log(VGATextModeColor color, char symbol, char *format, ...);

extern s32 test_engine_n_warning_expected;

__attr_noreturn void test_engine_shutdown();
void test_engine_run_tests(void);
