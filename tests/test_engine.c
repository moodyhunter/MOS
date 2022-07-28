// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine.h"

#include "mos/drivers/port.h"
#include "mos/drivers/screen.h"
#include "mos/kernel.h"
#include "mos/panic.h"
#include "mos/stdio.h"
#include "mos/stdlib.h"
#include "tinytest.h"

s32 test_engine_n_warning_expected = 0;

void mos_test_engine_log(VGATextModeColor color, char symbol, char *format, ...)
{
    char message[512];
    va_list args;
    va_start(args, format);
    vsprintf(message, format, args);
    va_end(args);

    color = tttttt(color);

    if (symbol)
    {
        screen_print_char('[');
        screen_print_char(symbol);
        screen_print_char(']');
    }
    else
        screen_print_string("   ");
    screen_print_char(' ');

    VGATextModeColor prev_fg;
    VGATextModeColor prev_bg;
    screen_get_color(&prev_fg, &prev_bg);
    {
        screen_set_color(color, Black);
        screen_print_string(message);
        screen_print_char('\n');
    }
    screen_set_color(prev_fg, prev_bg);
}

static void test_engine_warning_handler(const char *func, u32 line, const char *fmt, va_list args)
{
    char message[PRINTK_BUFFER_SIZE] = { 0 };
    if (test_engine_n_warning_expected > 0)
    {
        // MOS_TEST_LOG(MOS_TEST_BLUE, '\0', "expected warning: %s", msg);
    }
    else
    {
        vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
        pr_warn("warning: %s", message);
        pr_warn("  in function: %s (line %u)", func, line);
        mos_panic("unexpected warning");
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
    kwarn_handler_set(test_engine_warning_handler);

    TestResult result = { MOS_TEST_RESULT_INIT };

    MOS_TEST_FOREACH_TEST_CASE(testFunc)
    {
        TestResult r = { MOS_TEST_RESULT_INIT };
        (*testFunc)(&r);
        result.n_total += r.n_total;
        result.n_failed += r.n_failed;
        result.n_skipped += r.n_skipped;
    }

    kwarn_handler_remove();

    u32 passed = result.n_total - result.n_failed - result.n_skipped;

    if (result.n_failed == 0)
    {
        screen_set_color(Green, Black);
        pr_emph("\nALL %u TESTS PASSED: (%u succeed, %u failed, %u skipped)", result.n_total, passed, result.n_failed, result.n_skipped);
        test_engine_shutdown();
    }
    else
    {
        screen_set_color(Red, White);
        mos_panic("\nTEST FAILED: (%u succeed, %u failed, %u skipped)", passed, result.n_failed, result.n_skipped);
    }
}
