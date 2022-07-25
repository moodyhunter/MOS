// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine.h"

#include "mos/attributes.h"
#include "mos/drivers/port.h"
#include "mos/drivers/screen.h"
#include "mos/panic.h"
#include "mos/stdlib.h"
#include "tinytest.h"

extern const TestCase __start_mos_test_cases[];
extern const TestCase __stop_mos_test_cases[];

void print_hex(u32 value);

static __attr_noreturn void test_engine_panic_handler(const char *msg, const char *func, const char *file, const char *line)
{
    screen_set_color(Red, Black);
    TINY_LOG(TINY_RED, "KERNEL PANIC: %s, in function '%s' from file %s:%s\n", msg, func, file, line);
    while (1)
        ;
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
    bool success = true;
    for (const TestCase *test = __start_mos_test_cases; test != __stop_mos_test_cases; test++)
    {
        TestResult result = { TINY_TESTRESULT_INIT };
        test->body(&result);
        if (result.passed)
        {
            screen_set_color(Green, Black);
            printf("TEST PASSED: %s (total checks: %d)\n", test->name, result.checks);
            screen_set_color(Brown, Black);
        }
        else
        {
            success = false;
            screen_set_color(Red, Black);
            screen_print_string("TEST FAILED: ");
            screen_print_string(test->name);
            screen_print_string(", ");
            screen_set_color(White, Red);
            print_hex(result.failed_checks);
            screen_set_color(Red, Black);
            screen_print_string(" out of ");
            screen_set_color(White, Red);
            print_hex(result.checks);
            screen_set_color(Red, Black);
            screen_print_string(" checks has failed.\n");
        }
    }
    if (success)
    {
        screen_set_color(Green, Black);
        screen_print_string("ALL TESTS PASSED\n");
        test_engine_shutdown();
    }
    kpanic_handler_remove();
}
