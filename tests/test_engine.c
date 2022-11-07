// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/structures/list.h"
#include "mos/cmdline.h"
#include "mos/device/console.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "test_engine_impl.h"

s32 test_engine_n_warning_expected = 0;

void for_each_console_print_with_color(standard_color_t fg, standard_color_t bg, const char *message, size_t length)
{
    list_foreach(console_t, console, consoles)
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
        snprintf(prefix, 5, "[%c] ", symbol);
    else
        snprintf(prefix, 5, "    ");

    for_each_console_print_with_color(Gray, Black, prefix, 4);

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
        lprintk(MOS_LOG_WARN, "warning: %s", message);
        lprintk(MOS_LOG_WARN, "  in function: %s (line %u)\n", func, line);
        mos_panic("unexpected warning");
    }

    test_engine_n_warning_expected--;
}

void mos_test_engine_run_tests()
{
    kwarn_handler_set(test_engine_warning_handler);

    mos_test_result_t result = { MOS_TEST_RESULT_INIT };

    cmdline_arg_t *skip_tests_option = mos_cmdline_get_arg("mos_tests_skip");
    cmdline_arg_t *mos_tests_skip_prefix_option = mos_cmdline_get_arg("mos_tests_skip_prefix");

    MOS_TEST_FOREACH_TEST_CASE(test_case)
    {
        bool should_skip = false;
        for (u32 i = 0; skip_tests_option && i < skip_tests_option->params_count; i++)
        {
            cmdline_param_t *parameter = skip_tests_option->params[i];
            if (strcmp(parameter->val.string, test_case->test_name) == 0)
            {
                MOS_TEST_LOG(MOS_TEST_YELLOW, 'S', "Test %s skipped by kernel cmdline", test_case->test_name);
                should_skip = true;
                break;
            }
        }

        for (u32 i = 0; mos_tests_skip_prefix_option && i < mos_tests_skip_prefix_option->params_count; i++)
        {
            cmdline_param_t *parameter = mos_tests_skip_prefix_option->params[i];
            if (strncmp(parameter->val.string, test_case->test_name, strlen(parameter->val.string)) == 0)
            {
                MOS_TEST_LOG(MOS_TEST_YELLOW, 'S', "Test %s skipped by kernel cmdline (prefix %s)", test_case->test_name, parameter->val.string);
                should_skip = true;
                break;
            }
        }

        if (should_skip)
            continue;

        mos_test_result_t r = { MOS_TEST_RESULT_INIT };
        test_case->test_func(&r);

        result.n_total += r.n_total;
        result.n_failed += r.n_failed;
        result.n_skipped += r.n_skipped;

        if (result.n_failed > 0)
            mos_panic("TEST FAILED.");
    }

    kwarn_handler_remove();

    u32 passed = result.n_total - result.n_failed - result.n_skipped;
    pr_emph("ALL %u TESTS PASSED: (%u succeed, %u failed, %u skipped)", result.n_total, passed, result.n_failed, result.n_skipped);

    if (mos_cmdline_get_arg("mos_tests_halt_on_success"))
        mos_platform->halt_cpu();
    else
        mos_platform->shutdown();
}
