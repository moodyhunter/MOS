// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/platform/platform.hpp"
#include "test_engine_impl.h"

#include <mos/lib/structures/list.hpp>
#include <mos/misc/cmdline.hpp>
#include <mos/misc/panic.hpp>
#include <mos/misc/setup.hpp>
#include <mos/syslog/printk.hpp>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

s32 test_engine_n_warning_expected = 0;

static void test_engine_warning_handler(const char *func, u32 line, const char *fmt, va_list args)
{
    char message[MOS_PRINTK_BUFFER_SIZE];
    vsnprintf(message, MOS_PRINTK_BUFFER_SIZE, fmt, args);

    if (test_engine_n_warning_expected == 0)
    {
        lprintk(LogLevel::WARN, "\r\n");
        lprintk(LogLevel::WARN, "warning: %s", message);
        lprintk(LogLevel::WARN, "  in function: %s (line %u)", func, line);
        mos_panic("unexpected warning, test failed.");
    }

    test_engine_n_warning_expected--;
}

static const char **test_engine_skip_prefix_list = NULL;
static bool mos_tests_halt_on_success = false;

static bool mos_test_engine_setup_skip_prefix_list(mos::string_view arg)
{
    // split the argument into a list of strings
    int argc = 1;
    for (int i = 0; arg[i]; i++)
        if (arg[i] == ',')
            argc++;

    test_engine_skip_prefix_list = kcalloc<const char *>(argc);

    int i = 0;
    char *token = strtok((char *) arg.data(), ",");
    while (token)
    {
        test_engine_skip_prefix_list[i] = token;
        token = strtok(NULL, ",");
        i++;
    }

    return true;
}

MOS_SETUP("mos_tests_skip_prefix", mos_test_engine_setup_skip_prefix_list);

static bool mos_tests_setup_halt_on_success(mos::string_view arg)
{
    mos_tests_halt_on_success = cmdline_string_truthiness(arg, true);
    return true;
}

MOS_SETUP("mos_tests_halt_on_success", mos_tests_setup_halt_on_success);

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

static bool mos_test_engine_run_tests(mos::string_view arg)
{
    MOS_UNUSED(arg);
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

    if (mos_tests_halt_on_success)
        platform_halt_cpu();

    return true;
}

MOS_SETUP("mos_tests", mos_test_engine_run_tests);
