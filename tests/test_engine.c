// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "lib/structures/list.h"
#include "mos/cmdline.h"
#include "mos/device/console.h"
#include "mos/mm/kmalloc.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/setup.h"
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

        console_write(console, message, length);

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

static const char **test_engine_skip_prefix_list = NULL;
static bool test_engine_do_not_shutdown = false;

static bool mos_test_engine_setup_skip_prefix_list(int argc, const char **argv)
{
    test_engine_skip_prefix_list = kmalloc(sizeof(char *) * argc);
    for (int i = 0; i < argc; i++)
        test_engine_skip_prefix_list[i] = strdup(argv[i]);
    return true;
}

__setup(tests_skip, "mos_tests_skip_prefix", mos_test_engine_setup_skip_prefix_list);

static bool mos_test_engine_setup_do_not_shutdown(int argc, const char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    test_engine_do_not_shutdown = true;
    return true;
}

__setup(tests_halt_on_success, "mos_tests_halt_on_success", mos_test_engine_setup_do_not_shutdown);

static bool mos_test_engine_should_skip(const char *test_name)
{
    if (!test_engine_skip_prefix_list)
        return false;

    for (int i = 0; test_engine_skip_prefix_list[i]; i++)
    {
        if (strncmp(test_name, test_engine_skip_prefix_list[i], strlen(test_engine_skip_prefix_list[i])) == 0)
            return true;
    }

    return false;
}

static bool mos_test_engine_run_tests(int argc, const char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    kwarn_handler_set(test_engine_warning_handler);

    mos_test_result_t result = { 0 };

    MOS_TEST_FOREACH_TEST_CASE(test_case)
    {
        bool should_skip = mos_test_engine_should_skip(test_case->test_name);

        if (should_skip)
            continue;

        mos_test_result_t r = { 0 };
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

    if (test_engine_do_not_shutdown)
        platform_halt_cpu();
    else
        platform_shutdown();

    MOS_UNREACHABLE();
}

__setup(tests_run, "mos_tests", mos_test_engine_run_tests);
