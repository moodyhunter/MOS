// SPDX-License-Identifier: MIT
// Adapted from https://github.com/mateuszchudyk/tinytest

#pragma once

#include "mos/attributes.h"
#include "mos/drivers/screen.h"

#include <stdbool.h>

#ifndef MOS_TEST_STRCMP
int strcmp(const char *s1, const char *s2);
#define MOS_TEST_STRCMP strcmp
#endif

#ifndef MOS_TEST_PRINTF
int printf(const char *restrict format, ...);
#define MOS_TEST_PRINTF printf
#endif

typedef struct
{
    bool passed;
    unsigned checks;
    unsigned failures;
} TestResult;
typedef void (*mos_test_func_t)(TestResult *);

#define MOS_TEST_RESULT_INIT .passed = true, .checks = 0, .failures = 0

#define MOS_TEST_LOG(color, symbol, format, ...)                                                                                                \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (symbol)                                                                                                                             \
            MOS_TEST_PRINTF("[%c] ", symbol);                                                                                                   \
        else                                                                                                                                    \
            MOS_TEST_PRINTF("    ");                                                                                                            \
        screen_set_color(color, Black);                                                                                                         \
        MOS_TEST_PRINTF(format, __VA_ARGS__);                                                                                                   \
        MOS_TEST_PRINTF("\n");                                                                                                                  \
        screen_set_color(MOS_TEST_DEFAULT, Black);                                                                                              \
    } while (0)

#define MOS_TEST_GRAY    LightGray
#define MOS_TEST_RED     Red
#define MOS_TEST_GREEN   Green
#define MOS_TEST_YELLOW  Brown
#define MOS_TEST_BLUE    LightBlue
#define MOS_TEST_MAGENTA Magenta
#define MOS_TEST_CYAN    Cyan
#define MOS_TEST_DEFAULT MOS_TEST_GRAY

#define MOS_TEST_CASE(_TestName)                                                                                                                \
    static void _TestName(TestResult *);                                                                                                        \
    static void _MT_WRAP_TEST_NAME(_TestName)(TestResult * result)                                                                              \
    {                                                                                                                                           \
        MOS_TEST_LOG(MOS_TEST_BLUE, 'T', "Starting " #_TestName " at line %d", __LINE__);                                                       \
        _MT_RUN_TEST_AND_PRINT_RESULT(result, _TestName);                                                                                       \
    }                                                                                                                                           \
    _MT_REGISTER_TEST_CASE(_MT_WRAP_TEST_NAME(_TestName));                                                                                      \
    static void _TestName(TestResult *_MT_result)

#define MOS_TEST_DECL_PTEST(_PTestName, ptest_args_printf_format, ...)                                                                          \
    static const char *_MT_PTEST_ARG_FORMAT(_PTestName) = "argument: " ptest_args_printf_format;                                                \
    static void _PTestName(TestResult *_MT_result, __VA_ARGS__)

#define MOS_TEST_PTEST_INSTANCE(_PTestName, ...)                                                                                                \
    static void _MT_PTEST_CALLER(_PTestName)(TestResult * result)                                                                               \
    {                                                                                                                                           \
        _PTestName(result, __VA_ARGS__);                                                                                                        \
    }                                                                                                                                           \
    static void _MT_WRAP_PTEST_CALLER(_PTestName)(TestResult * result)                                                                          \
    {                                                                                                                                           \
        MOS_TEST_LOG(MOS_TEST_BLUE, 'P', "Starting parameterised test %s at line %d", #_PTestName, __LINE__);                                   \
        MOS_TEST_LOG(MOS_TEST_BLUE, '\0', _MT_PTEST_ARG_FORMAT(_PTestName), __VA_ARGS__);                                                       \
        _MT_RUN_TEST_AND_PRINT_RESULT(result, _MT_PTEST_CALLER(_PTestName));                                                                    \
    }                                                                                                                                           \
    _MT_REGISTER_TEST_CASE(_MT_WRAP_PTEST_CALLER(_PTestName));

#define MOS_TEST_EXPECT_WARNING(body, msg)                                                                                                      \
    do                                                                                                                                          \
    {                                                                                                                                           \
        test_engine_kwarning_expected = true;                                                                                                   \
        body;                                                                                                                                   \
        test_engine_kwarning_expected = false;                                                                                                  \
        if (!test_engine_kwarning_seen)                                                                                                         \
            MOS_TEST_FAIL("expected warning not seen: %s, line %d", msg, __LINE__);                                                             \
        test_engine_kwarning_seen = false;                                                                                                      \
    } while (0)

/*
 * Test force fail
 *
 * Arguments:
 *   - format           - message printf format
 *   - args... [opt]    - arguments
 */
#define MOS_TEST_FAIL(format, ...)                                                                                                              \
    do                                                                                                                                          \
    {                                                                                                                                           \
        MOS_TEST_LOG(MOS_TEST_RED, 'X', format, __VA_ARGS__);                                                                                   \
        _MT_result->passed = false;                                                                                                             \
    } while (false)

/*
 * Check if actual value is equal expected value.
 *
 * Arguments:
 *   - expected         - expected value
 *   - actual           - actual value
 */
#define MOS_TEST_CHECK(expected, actual)                                                                                                        \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_MT_result->checks;                                                                                                                   \
        if ((expected) != (actual))                                                                                                             \
        {                                                                                                                                       \
            MOS_TEST_FAIL("values are different (expected = %d, actual = %d)", (expected), (actual));                                           \
            ++_MT_result->failures;                                                                                                             \
        }                                                                                                                                       \
    } while (false)

/*
 * Check if actual string is equal expected string.
 *
 * Arguments:
 *   - expected         - expected string
 *   - actual           - actual string
 */
#define MOS_TEST_CHECK_STRING(expected, actual)                                                                                                 \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_MT_result->checks;                                                                                                                   \
        if (MOS_TEST_STRCMP(expected, actual) != 0)                                                                                             \
        {                                                                                                                                       \
            MOS_TEST_FAIL("values are different (expected = '%s', actual = '%s'), at line %u", (expected), (actual), (__LINE__));               \
            ++_MT_result->failures;                                                                                                             \
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
#define MOS_TEST_CHECK_EPS(expected, actual, epsilon)                                                                                           \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_MT_result->checks;                                                                                                                   \
        if (_MT_FLOATABS((expected) - (actual)) > (epsilon))                                                                                    \
        {                                                                                                                                       \
            MOS_TEST_FAIL("values differ by more then %f (expected = %f, actual = %f)", (epsilon), (expected), (actual));                       \
            ++_MT_result->failures;                                                                                                             \
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
#define MOS_TEST_CHECK_ARRAY(expected, actual, elements)                                                                                        \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_MT_result->checks;                                                                                                                   \
        bool failed = false;                                                                                                                    \
        for (unsigned i = 0; i < (unsigned) (elements); ++i)                                                                                    \
            if ((expected)[i] != (actual)[i])                                                                                                   \
            {                                                                                                                                   \
                MOS_TEST_FAIL("memories differ at %u-th position (expected = %d, actual = %d)", i, (expected)[i], (actual)[i]);                 \
                failed = true;                                                                                                                  \
            }                                                                                                                                   \
        if (failed)                                                                                                                             \
            ++_MT_result->failures;                                                                                                             \
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
#define MOS_TEST_CHECK_ARRAY_EPS(expected, actual, elements, epsilon)                                                                           \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_MT_result->checks;                                                                                                                   \
        bool failed = false;                                                                                                                    \
        for (unsigned i = 0; i < (unsigned) (elements); ++i)                                                                                    \
            if (_MT_FLOATABS((expected)[i] - (actual)[i]) > (epsilon))                                                                          \
            {                                                                                                                                   \
                MOS_TEST_FAIL("memories differ at %u-th position by more then %f (expected = %f, actual = %f)", i, (epsilon), (expected)[i],    \
                              (actual)[i]);                                                                                                     \
                failed = true;                                                                                                                  \
            }                                                                                                                                   \
        if (failed)                                                                                                                             \
            ++_MT_result->failures;                                                                                                             \
    } while (false)

#define _MT_RUN_TEST_AND_PRINT_RESULT(_ResultVar, _TestFunc)                                                                                    \
    _TestFunc(_ResultVar);                                                                                                                      \
    if (_ResultVar->passed)                                                                                                                     \
        MOS_TEST_LOG(MOS_TEST_GREEN, '\0', "Passed (%u/%u)", _ResultVar->checks, _ResultVar->checks);                                           \
    else                                                                                                                                        \
        MOS_TEST_LOG(MOS_TEST_RED, 'X', "Failed (%u/%u)", _ResultVar->failures, _ResultVar->checks);

#define _MT_FLOATABS(a) ((a) < 0 ? -(a) : (a))

// Wrapper for the simple test
#define _MT_WRAP_TEST_NAME(test_name) __mos_test_wrapped_test_##test_name

// Wrapper for the parameterized test
#define _MT_PTEST_ARG_FORMAT(ptest_name)  __mos_test_ptest_args_format_##ptest_name
#define _MT_PTEST_CALLER(ptest_name)      MOS_CONCAT(__mos_test_ptest_caller_##ptest_name, __LINE__)
#define _MT_WRAP_PTEST_CALLER(ptest_name) MOS_CONCAT(__mos_test_wrapped_ptest_caller_##ptest_name, __LINE__)

// ELF Section based test registration
#define _MT_REGISTER_TEST_CASE(_TFunc)    const mos_test_func_t __section(mos_test_cases) MOS_CONCAT(test_cases_L, __LINE__) = _TFunc
#define MOS_TEST_FOREACH_TEST_CASE(_FPtr) for (const mos_test_func_t *_FPtr = __start_mos_test_cases; _FPtr != __stop_mos_test_cases; _FPtr++)

// Defined by the linker, do not rename.
extern const mos_test_func_t __start_mos_test_cases[];
extern const mos_test_func_t __stop_mos_test_cases[];
