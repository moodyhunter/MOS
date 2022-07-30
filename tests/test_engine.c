// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine.h"

#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/device/console.h"
#include "mos/kernel.h"
#include "mos/panic.h"

s32 test_engine_n_warning_expected = 0;

void for_each_console_print_with_color(standard_color_t fg, standard_color_t bg, const char *message, size_t length)
{
    extern console_t *platform_consoles;
    list_foreach(platform_consoles, console)
    {
        standard_color_t prev_fg, prev_bg;
        if (console->caps & CONSOLE_CAP_COLOR)
        {
            console->get_color(console, &prev_fg, &prev_bg);
            console->set_color(console, fg, bg);
        }

        console->write(console, message, length);

        if (console->caps & CONSOLE_CAP_COLOR)
        {
            console->set_color(console, prev_fg, prev_bg);
        }
    }
}

void mos_test_engine_log(standard_color_t color, char symbol, char *format, ...)
{
    char prefix[5];

    if (symbol)
        snprintf(prefix, 5, "[%c] ", symbol ? symbol : ' ');
    else
        snprintf(prefix, 5, "    ");

    for_each_console_print_with_color(LightGray, Black, prefix, 5);

    char message[512];
    va_list args;
    va_start(args, format);
    vsprintf(message, format, args);
    va_end(args);

    for_each_console_print_with_color(color, Black, message, strlen(message));
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
        pr_emph("\nALL %u TESTS PASSED: (%u succeed, %u failed, %u skipped)", result.n_total, passed, result.n_failed, result.n_skipped);
        mos_platform.platform_shutdown();
        while (1)
            ;
    }
    else
    {
        mos_panic("\nTEST FAILED: (%u succeed, %u failed, %u skipped)", passed, result.n_failed, result.n_skipped);
    }
}
