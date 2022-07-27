// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine.h"

#include "mos/attributes.h"
#include "mos/bug.h"
#include "mos/drivers/port.h"
#include "mos/drivers/screen.h"
#include "mos/panic.h"
#include "mos/stdlib.h"
#include "tinytest.h"

s32 test_engine_n_warning_expected = 0;

static __attr_noreturn void test_engine_panic_handler(const char *msg, const char *func, const char *file, const char *line)
{
    screen_set_color(White, Red);
    printf("PANIC: %s\n", msg);
    screen_set_color(Red, Black);
    printf("  function '%s'\n  file %s:%s\n", func, file, line);
    while (1)
        ;
}

static void test_engine_warning_handler(const char *msg, const char *func, const char *file, const char *line)
{
    if (test_engine_n_warning_expected > 0)
    {
        MOS_TEST_LOG(MOS_TEST_BLUE, '\0', "expected warning: %s", msg);
    }
    else
    {
        screen_set_color(White, Brown);
        printf("WARN: %s", msg);
        screen_set_color(Brown, Black);
        printf("\n  function '%s'\n  file %s:%s\n", func, file, line);
        screen_set_color(LightGray, Black);
        panic("unexpected warning");
    }

    test_engine_n_warning_expected--;
}

__attr_noreturn void test_engine_shutdown()
{
    port_outw(0x604, 0x2000);
    while (1)
        ;
}

void test_engine_run_tests()
{
    kpanic_handler_set(test_engine_panic_handler);
    kwarn_handler_set(test_engine_warning_handler);
    TestResult result = { MOS_TEST_RESULT_INIT };
    MOS_TEST_FOREACH_TEST_CASE(testFunc)
    {
        (*testFunc)(&result);
    }

    kpanic_handler_remove();
    kwarn_handler_remove();

    if (result.failures == 0)
    {
        screen_set_color(Green, Black);
        printf("\nALL TESTS PASSED: (%u succeed, %u failed, %u skipped)\n", result.checks - result.failures, result.failures, result.skipped);
        test_engine_shutdown();
    }
    else
    {
        screen_set_color(Red, White);
        printf("\nTEST FAILED: (%u succeed, %u failed, %u skipped)\n", result.checks - result.failures, result.failures, result.skipped);
    }
}
