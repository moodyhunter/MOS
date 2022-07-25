// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/attributes.h"

__attr_noreturn void test_engine_shutdown();
void test_engine_run_tests(void);

int printf(const char *format, ...);
#define TINY_TEST_PRINTF(arg1, ...) printf(arg1, __VA_ARGS__)
#include "tinytest.h"
