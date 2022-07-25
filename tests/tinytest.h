// SPDX-License-Identifier: MIT
// https://github.com/mateuszchudyk/tinytest
// License: MIT

#pragma once

#include "mos/attributes.h"

#include <stdbool.h>

#define TINY_TEST_NAME    "TinyTest"
#define TINY_TEST_VERSION "0.4.0"

#ifndef TINY_TEST_STRCMP
int strcmp(const char *s1, const char *s2);
#define TINY_TEST_STRCMP strcmp
#endif

#ifndef TINY_TEST_PRINTF
int printf(const char *restrict format, ...);
#define TINY_TEST_PRINTF printf
#endif

/*
 * Set color formatting for the text.
 */
#ifdef PRINTF_HAS_COLOR
#define TINY_COLOR(color, text) color text "\x1b[0m"

#define TINY_DEFAULT "\x1b[0m"
#define TINY_GRAY    "\x1b[90m"
#define TINY_RED     "\x1b[91m"
#define TINY_GREEN   "\x1b[92m"
#define TINY_YELLOW  "\x1b[93m"
#define TINY_BLUE    "\x1b[94m"
#define TINY_MAGENTA "\x1b[95m"
#define TINY_CYAN    "\x1b[96m"
#else
#define TINY_COLOR(color, text) text

#define TINY_DEFAULT ""
#define TINY_GRAY    ""
#define TINY_RED     ""
#define TINY_GREEN   ""
#define TINY_YELLOW  ""
#define TINY_BLUE    ""
#define TINY_MAGENTA ""
#define TINY_CYAN    ""
#endif

/*
 * Print log.
 *
 * Arguments:
 *   - color            - log color
 *   - format           - printf format
 *   - args... [opt]    - arguments
 */
#define TINY_LOG(color, ...) _TT_TINY_LOG(color, __VA_ARGS__)
#define TINY_TEST(test_name)                                                                                                                    \
    static void test_name(TestResult *);                                                                                                        \
    _TT_APPEND_TEST(test_name, test_name)                                                                                                       \
    static void test_name(__attribute__((unused)) TestResult *_tt_result)

/*
 * Create a parametrized test.
 *
 * Arguments:
 *   - ptest_name
 *   - ptest_args_printf_format
 *   - ptest_args...
 */
#define TINY_PTEST(ptest_name, ptest_args_printf_format, ...)                                                                                   \
    static const char *_tt_ptest_args_format_##ptest_name = ptest_args_printf_format;                                                           \
    static void ptest_name(__attribute__((unused)) TestResult *_tt_result, __VA_ARGS__)

/*
 * Create a test that is an instance of the given ptest.
 *
 * Arguments:
 *   - ptest_name
 *   - ptest_args...
 */
#define TINY_PTEST_INSTANCE(ptest_name, ...)                                                                                                    \
    static void _TT_CONCAT(_tt_ptest_instance_##ptest_name, __LINE__)(TestResult *);                                                            \
    _TT_APPEND_TEST(ptest_name, _TT_CONCAT(_tt_ptest_instance_##ptest_name, __LINE__), _tt_ptest_args_format_##ptest_name, __VA_ARGS__);        \
    static void _TT_CONCAT(_tt_ptest_instance_##ptest_name, __LINE__)(TestResult * result)                                                      \
    {                                                                                                                                           \
        ptest_name(result, __VA_ARGS__);                                                                                                        \
    }

/*
 * Test force fail
 *
 * Arguments:
 *   - format           - message printf format
 *   - args... [opt]    - arguments
 */
#define TINY_FAIL(...)                                                                                                                          \
    do                                                                                                                                          \
    {                                                                                                                                           \
        TINY_LOG(TINY_RED, __VA_ARGS__);                                                                                                        \
        _tt_result->passed = false;                                                                                                             \
    } while (false)

/*
 * Check if actual value is equal expected value.
 *
 * Arguments:
 *   - expected         - expected value
 *   - actual           - actual value
 */
#define TINY_CHECK(expected, actual)                                                                                                            \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_tt_result->checks;                                                                                                                   \
        if ((expected) != (actual))                                                                                                             \
        {                                                                                                                                       \
            TINY_FAIL("values are different (expected = %d, actual = %d)", (expected), (actual));                                               \
            ++_tt_result->failed_checks;                                                                                                        \
        }                                                                                                                                       \
    } while (false)

/*
 * Check if actual string is equal expected string.
 *
 * Arguments:
 *   - expected         - expected string
 *   - actual           - actual string
 */
#define TINY_CHECK_STRING(expected, actual)                                                                                                     \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_tt_result->checks;                                                                                                                   \
        if (TINY_TEST_STRCMP(expected, actual) != 0)                                                                                            \
        {                                                                                                                                       \
            TINY_FAIL("values are different (expected = '%s', actual = '%s')", (expected), (actual));                                           \
            ++_tt_result->failed_checks;                                                                                                        \
        }                                                                                                                                       \
    } while (false)

/*
 * Check if actual value is not differ from expected value by more then epsilon.
 *
 * Arguments:
 *   - expected         - expected value
 *   - actual           - actual value
 *   - epsilon          - maximum acceptable difference
 */
#define TINY_CHECK_EPS(expected, actual, epsilon)                                                                                               \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_tt_result->checks;                                                                                                                   \
        if (_TT_FABS((expected) - (actual)) > (epsilon))                                                                                        \
        {                                                                                                                                       \
            TINY_FAIL("values differ by more then %f (expected = %f, actual = %f)", (epsilon), (expected), (actual));                           \
            ++_tt_result->failed_checks;                                                                                                        \
        }                                                                                                                                       \
    } while (false)

/*
 * Check if every value in the actual array is equal to corresponding value
 * in the expected array.
 *
 * Arguments:
 *   - expected         - expected array pointer
 *   - actual           - actual array pointer
 *   - elements         - number of array elements
 */
#define TINY_CHECK_ARRAY(expected, actual, elements)                                                                                            \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_tt_result->checks;                                                                                                                   \
        bool failed = false;                                                                                                                    \
        for (unsigned i = 0; i < (unsigned) (elements); ++i)                                                                                    \
            if ((expected)[i] != (actual)[i])                                                                                                   \
            {                                                                                                                                   \
                TINY_FAIL("memories differ at %u-th position (expected = %d, actual = %d)", i, (expected)[i], (actual)[i]);                     \
                failed = true;                                                                                                                  \
            }                                                                                                                                   \
        if (failed)                                                                                                                             \
            ++_tt_result->failed_checks;                                                                                                        \
    } while (false)

/*
 * Check if every value in the actual array is not differ from corresponding value
 * in the expected array by more then epsilon.
 *
 * Arguments:
 *   - expected         - expected array pointer
 *   - actual           - actual array pointer
 *   - epsilon          - maximum acceptable difference
 *   - elements         - number of array elements
 */
#define TINY_CHECK_ARRAY_EPS(expected, actual, elements, epsilon)                                                                               \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_tt_result->checks;                                                                                                                   \
        bool failed = false;                                                                                                                    \
        for (unsigned i = 0; i < (unsigned) (elements); ++i)                                                                                    \
            if (_TT_FABS((expected)[i] - (actual)[i]) > (epsilon))                                                                              \
            {                                                                                                                                   \
                TINY_FAIL("memories differ at %u-th position by more then %f (expected = %f, actual = %f)", i, (epsilon), (expected)[i],        \
                          (actual)[i]);                                                                                                         \
                failed = true;                                                                                                                  \
            }                                                                                                                                   \
        if (failed)                                                                                                                             \
            ++_tt_result->failed_checks;                                                                                                        \
    } while (false)

// Floating point absolute value
#define _TT_FABS(a)            ((a) < 0 ? -(a) : (a))
#define _TT_CONCAT_INNER(a, b) a##b
#define _TT_CONCAT(a, b)       _TT_CONCAT_INNER(a, b)

#define _TT_TINY_LOG(color, format, ...) TINY_TEST_PRINTF("[   ] " TINY_COLOR(color, "Line #%d: " format "\n"), __LINE__, __VA_ARGS__)

#define _TT_APPEND_TEST(test_name, test_body)                                                                                                   \
    static __attr_used void _tt_test_##test_body(TestResult *result)                                                                            \
    {                                                                                                                                           \
        TINY_TEST_PRINTF("[ TEST ] " #test_name " -- " __FILE__ ":%d\n", __LINE__);                                                             \
        test_body(result);                                                                                                                      \
        if (result->passed)                                                                                                                     \
            TINY_TEST_PRINTF("[===] " TINY_COLOR(TINY_GREEN, "Passed (%u/%u)\n"), result->checks, result->checks);                              \
        else                                                                                                                                    \
            TINY_TEST_PRINTF("[XXX] " TINY_COLOR(TINY_RED, "Failed (%u/%u)\n"), result->failed_checks, result->checks);                         \
    }

typedef struct
{
    bool passed;
    unsigned checks;
    unsigned failed_checks;
} TestResult;
#define TINY_TESTRESULT_INIT .passed = true, .checks = 0, .failed_checks = 0

typedef void (*TestBody)(TestResult *);

typedef struct
{
    char name[16];
    TestBody body;
} __attr_aligned(32) TestCase;

#define TINY_TEST_CASES const TestCase __section(mos_test_cases)
