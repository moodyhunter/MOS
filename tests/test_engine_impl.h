// SPDX-License-Identifier: MIT
// Adapted from https://github.com/mateuszchudyk/tinytest

#pragma once

#include "mos/device/console.h"
#include "mos/mos_global.h"
#include "mos/types.h"

#include <stdbool.h>

extern s32 test_engine_n_warning_expected;
extern void mos_test_engine_log(standard_color_t color, char symbol, char *format, ...);

#ifndef MOS_TEST_LOG
#define MOS_TEST_LOG(color, symbol, format, ...) mos_test_engine_log(color, symbol, format "\n", __VA_ARGS__)
#endif

#ifndef MOS_TEST_STRCMP
int strcmp(const char *s1, const char *s2);
#define MOS_TEST_STRCMP strcmp
#endif

typedef struct
{
    u32 n_total;
    u32 n_failed;
    u32 n_skipped;
} TestResult;
typedef void (*mos_test_func_t)(TestResult *);

#define MOS_TEST_RESULT_INIT .n_total = 0, .n_failed = 0, .n_skipped = 0

#define MOS_TEST_GRAY    LightGray
#define MOS_TEST_RED     Red
#define MOS_TEST_GREEN   Green
#define MOS_TEST_YELLOW  Brown
#define MOS_TEST_BLUE    LightBlue
#define MOS_TEST_MAGENTA Magenta
#define MOS_TEST_CYAN    Cyan
#define MOS_TEST_DEFAULT MOS_TEST_GRAY

#define MOS_TEST_DEFINE_CONDITION(condition, message)                                                                                           \
    const char *_mt_test_cond_##condition##_message = message;                                                                                  \
    static bool condition

#define MOS_TEST_CONDITIONAL(cond)                                                                                                              \
    for (MOS_TEST_CURRENT_TEST_SKIPPED = !(cond), (*_mt_loop_leave) = false, __extension__({                                                    \
             if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                 \
                 MOS_TEST_LOG(MOS_TEST_BLUE, '\0', "Skipped '%s': condition '%s' not met.", _mt_test_cond_##cond##_message, #cond);             \
         });                                                                                                                                    \
         !(*_mt_loop_leave); (*_mt_loop_leave) = true, MOS_TEST_CURRENT_TEST_SKIPPED = false)

#define MOS_TEST_SKIP()                                                                                                                         \
    do                                                                                                                                          \
    {                                                                                                                                           \
        ++_MT_result->n_total;                                                                                                                  \
        ++_MT_result->n_skipped;                                                                                                                \
    } while (0)

#define MOS_TEST_CASE(_TestName)                                                                                                                \
    static void _TestName(TestResult *, bool *, bool *);                                                                                        \
    static void _MT_WRAP_TEST_NAME(_TestName)(TestResult * result)                                                                              \
    {                                                                                                                                           \
        MOS_TEST_LOG(MOS_TEST_BLUE, 'T', "Starting test " #_TestName " (line %d)", __LINE__);                                                   \
        _MT_RUN_TEST_AND_PRINT_RESULT(result, _TestName);                                                                                       \
    }                                                                                                                                           \
    _MT_REGISTER_TEST_CASE(_TestName, _MT_WRAP_TEST_NAME(_TestName));                                                                           \
    static void _TestName(TestResult *_MT_result, __unused bool *_mt_test_skipped, __unused bool *_mt_loop_leave)

#define MOS_TEST_DECL_PTEST(_PTestName, ptest_args_printf_format, ...)                                                                          \
    static const char *_MT_PTEST_ARG_FORMAT(_PTestName) = "argument: " ptest_args_printf_format;                                                \
    static void _PTestName(TestResult *_MT_result, __unused bool *_mt_test_skipped, __unused bool *_mt_loop_leave, __VA_ARGS__)

#define MOS_TEST_PTEST_INSTANCE(_PTestName, ...)                                                                                                \
    static void _MT_PTEST_CALLER(_PTestName)(TestResult * result, bool *_mt_test_skipped, bool *_mt_loop_leave)                                 \
    {                                                                                                                                           \
        _PTestName(result, _mt_test_skipped, _mt_loop_leave, __VA_ARGS__);                                                                      \
    }                                                                                                                                           \
    static void _MT_WRAP_PTEST_CALLER(_PTestName)(TestResult * result)                                                                          \
    {                                                                                                                                           \
        MOS_TEST_LOG(MOS_TEST_BLUE, 'P', "Starting parameterised test %s at line %d", #_PTestName, __LINE__);                                   \
        MOS_TEST_LOG(MOS_TEST_BLUE, '\0', _MT_PTEST_ARG_FORMAT(_PTestName), __VA_ARGS__);                                                       \
        _MT_RUN_TEST_AND_PRINT_RESULT(result, _MT_PTEST_CALLER(_PTestName));                                                                    \
    }                                                                                                                                           \
    _MT_REGISTER_TEST_CASE(_TestName, _MT_WRAP_PTEST_CALLER(_PTestName));

#define MOS_TEST_EXPECT_WARNING_N(N, body, msg)                                                                                                 \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                      \
        {                                                                                                                                       \
            MOS_TEST_SKIP();                                                                                                                    \
            break;                                                                                                                              \
        }                                                                                                                                       \
        test_engine_n_warning_expected = N;                                                                                                     \
        body;                                                                                                                                   \
        if (test_engine_n_warning_expected != 0)                                                                                                \
            MOS_TEST_FAIL("%d more expected warning(s) not seen: %s, line %d", test_engine_n_warning_expected, msg, __LINE__);                  \
    } while (0)

#define MOS_TEST_EXPECT_WARNING(body, msg) MOS_TEST_EXPECT_WARNING_N(1, body, msg)
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
        ++_MT_result->n_failed;                                                                                                                 \
        MOS_TEST_LOG(MOS_TEST_RED, 'X', "line %d: " format, __LINE__, __VA_ARGS__);                                                             \
    } while (false)

/*
 * Assertion
 *
 * Arguments:
 *   - condition       - condition to test
 *   - format           - message printf format
 *   - args... [opt]    - arguments
 */
#define MOS_TEST_ASSERT(condition, format, ...)                                                                                                 \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                      \
        {                                                                                                                                       \
            MOS_TEST_SKIP();                                                                                                                    \
            break;                                                                                                                              \
        }                                                                                                                                       \
        ++_MT_result->n_total;                                                                                                                  \
        if (!(condition))                                                                                                                       \
            MOS_TEST_FAIL("ASSERTION FAILED: %s, " format, #condition, ##__VA_ARGS__);                                                          \
    } while (false)

/*
 * Check if actual value is equal expected value.
 *
 * Arguments:
 *   - expected         - expected value
 *   - actual           - actual value
 */
#define MOS_TEST_CHECK(actual, expected)                                                                                                        \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                      \
        {                                                                                                                                       \
            MOS_TEST_SKIP();                                                                                                                    \
            break;                                                                                                                              \
        }                                                                                                                                       \
        ++_MT_result->n_total;                                                                                                                  \
        if ((expected) != (actual))                                                                                                             \
            MOS_TEST_FAIL("'%s' is %d, expected %d", #actual, (actual), (expected));                                                            \
    } while (false)

/*
 * Check if actual string is equal expected string.
 *
 * Arguments:
 *   - expected         - expected string
 *   - actual           - actual string
 */
#define MOS_TEST_CHECK_STRING(actual, expected)                                                                                                 \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                      \
        {                                                                                                                                       \
            MOS_TEST_SKIP();                                                                                                                    \
            break;                                                                                                                              \
        }                                                                                                                                       \
        ++_MT_result->n_total;                                                                                                                  \
        if (MOS_TEST_STRCMP(expected, actual) != 0)                                                                                             \
            MOS_TEST_FAIL("values are different (expected = '%s', actual = '%s'), at line %u", (expected), (actual), (__LINE__));               \
    } while (false)

/*
 * Check if actual value is not differ from expected value by more then epsilon.
 *
 * Arguments:
 *   - expected         - expected value
 *   - actual           - actual value
 *   - epsilon          - maximum acceptable difference
 */
#define MOS_TEST_CHECK_EPS(actual, expected, epsilon)                                                                                           \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                      \
        {                                                                                                                                       \
            MOS_TEST_SKIP();                                                                                                                    \
            break;                                                                                                                              \
        }                                                                                                                                       \
        ++_MT_result->n_total;                                                                                                                  \
        if (_MT_FLOATABS((expected) - (actual)) > (epsilon))                                                                                    \
            MOS_TEST_FAIL("values differ by more then %f (expected = %f, actual = %f)", (epsilon), (expected), (actual));                       \
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
#define MOS_TEST_CHECK_ARRAY(actual, expected, elements)                                                                                        \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                      \
        {                                                                                                                                       \
            MOS_TEST_SKIP();                                                                                                                    \
            break;                                                                                                                              \
        }                                                                                                                                       \
        ++_MT_result->n_total;                                                                                                                  \
        for (unsigned i = 0; i < (unsigned) (elements); ++i)                                                                                    \
        {                                                                                                                                       \
            if ((expected)[i] != (actual)[i])                                                                                                   \
            {                                                                                                                                   \
                MOS_TEST_FAIL("memories differ at %u-th position (expected = %d, actual = %d)", i, (expected)[i], (actual)[i]);                 \
                break;                                                                                                                          \
            }                                                                                                                                   \
        }                                                                                                                                       \
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
#define MOS_TEST_CHECK_ARRAY_EPS(actual, expected, elements, epsilon)                                                                           \
    do                                                                                                                                          \
    {                                                                                                                                           \
        if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                      \
        {                                                                                                                                       \
            MOS_TEST_SKIP();                                                                                                                    \
            break;                                                                                                                              \
        }                                                                                                                                       \
        ++_MT_result->n_total;                                                                                                                  \
        for (unsigned i = 0; i < (unsigned) (elements); ++i)                                                                                    \
        {                                                                                                                                       \
            if (_MT_FLOATABS((expected)[i] - (actual)[i]) > (epsilon))                                                                          \
            {                                                                                                                                   \
                MOS_TEST_FAIL("memories differ at %u by more then %f (expected = %f, actual = %f)", i, (epsilon), (expected)[i], (actual)[i]);  \
                break;                                                                                                                          \
            }                                                                                                                                   \
        }                                                                                                                                       \
    } while (false)

#define _MT_RUN_TEST_AND_PRINT_RESULT(_ResultVar, _TestFunc)                                                                                    \
    do                                                                                                                                          \
    {                                                                                                                                           \
        bool _mt_test_skipped = false;                                                                                                          \
        bool _mt_loop_leave = false;                                                                                                            \
        _TestFunc(_ResultVar, &_mt_test_skipped, &_mt_loop_leave);                                                                              \
        u32 total = _ResultVar->n_total;                                                                                                        \
        u32 failed = _ResultVar->n_failed;                                                                                                      \
        u32 skipped = _ResultVar->n_skipped;                                                                                                    \
        u32 passed = total - failed - skipped;                                                                                                  \
        if (failed == 0)                                                                                                                        \
            if (skipped == 0)                                                                                                                   \
                MOS_TEST_LOG(MOS_TEST_GREEN, '\0', "%s: All %u test(s) passed", #_TestFunc, total);                                             \
            else                                                                                                                                \
                MOS_TEST_LOG(MOS_TEST_GREEN, '\0', "%s: All %u test(s) passed (%u skipped)", #_TestFunc, total, skipped);                       \
        else                                                                                                                                    \
            MOS_TEST_LOG(MOS_TEST_RED, 'X', "%s: %u out of %u test(s) failed (%u passed, %u skipped)", #_TestFunc, failed, total, passed,       \
                         skipped);                                                                                                              \
    } while (0)

#define _MT_FLOATABS(a) ((a) < 0 ? -(a) : (a))

#define MOS_TEST_CURRENT_TEST_SKIPPED (*_mt_test_skipped)

// Wrapper for the simple test
#define _MT_WRAP_TEST_NAME(test_name) __mos_test_wrapped_test_##test_name

// Wrapper for the parameterized test
#define _MT_PTEST_ARG_FORMAT(ptest_name)  __mos_test_ptest_args_format_##ptest_name
#define _MT_PTEST_CALLER(ptest_name)      MOS_CONCAT(__mos_test_ptest_caller_##ptest_name, __LINE__)
#define _MT_WRAP_PTEST_CALLER(ptest_name) MOS_CONCAT(__mos_test_wrapped_ptest_caller_##ptest_name, __LINE__)

// ELF Section based test registration
#define _MT_REGISTER_TEST_CASE(_TName, _TFunc)                                                                                                  \
    const mos_test_func_t __section(mos_test_cases) MOS_CONCAT(test_cases_##_TName##_L, __LINE__) = _TFunc
#define MOS_TEST_FOREACH_TEST_CASE(_FPtr) for (const mos_test_func_t *_FPtr = __start_mos_test_cases; _FPtr != __stop_mos_test_cases; _FPtr++)

// Defined by the linker, do not rename.
extern const mos_test_func_t __start_mos_test_cases[];
extern const mos_test_func_t __stop_mos_test_cases[];
