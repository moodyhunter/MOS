// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/attributes.h"
#include "tinytest.h"

extern s32 test_engine_n_warning_expected;

__attr_noreturn void test_engine_shutdown();
void test_engine_run_tests(void);
