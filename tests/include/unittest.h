// SPDX-License-Identifier: MIT
// https://github.com/mateuszchudyk/tinytest
// License: MIT

#ifndef TINY_TEST_H
#define TINY_TEST_H

//------------------------------------------------------------------------------
//
// TinyTest information
//
//------------------------------------------------------------------------------

#define TINY_TEST_NAME    "TinyTest"
#define TINY_TEST_VERSION "0.4.0"

//------------------------------------------------------------------------------
//
// You can define following macros by your own to customize TinyTest:
//   - TINY_TEST_MAX_TESTS                  - maximum number of tests
//   - TINY_TEST_PRINTF(format, ...)        - printing function
//
//------------------------------------------------------------------------------

#ifndef TINY_TEST_MAX_TESTS
#define TINY_TEST_MAX_TESTS 1000
#endif

#ifndef TINY_TEST_PRINTF
#include <stdio.h>
#define TINY_TEST_PRINTF(format, ...) printf(format, __VA_ARGS__)
#include <string.h>
#define TINY_TEST_STRCMP(a, b) strcmp(a, b)
#endif

//------------------------------------------------------------------------------
//
// Logs
//
//------------------------------------------------------------------------------

/*
 * Set color formatting for the text.
 */
#define TINY_COLOR(color, text) color text "\x1b[0m"

#define TINY_DEFAULT "\x1b[0m"
#define TINY_GRAY    "\x1b[90m"
#define TINY_RED     "\x1b[91m"
#define TINY_GREEN   "\x1b[92m"
#define TINY_YELLOW  "\x1b[93m"
#define TINY_BLUE    "\x1b[94m"
#define TINY_MAGENTA "\x1b[95m"
#define TINY_CYAN    "\x1b[96m"

/*
 * Print log.
 *
 * Arguments:
 *   - color            - log color
 *   - format           - printf format
 *   - args... [opt]    - arguments
 */
#define TINY_LOG(color, ...) _TT_CHOOSE_WRAPPER(_TT_TINY_LOG, _TT_AT_LEAST_1_ARG(__VA_ARGS__), color, __VA_ARGS__)

//------------------------------------------------------------------------------
//
// Tests
//
//------------------------------------------------------------------------------

/*
 * Create a basic test.
 * It's automatically added to the tests queue (see: TINY_TEST_RUN_ALL). It can
 * be also run as a separate single tests (see: TINY_TEST_RUN_TEST).
 *
 * Arguments:
 *   - test_name
 */
#define TINY_TEST(test_name)                                                                                                                    \
    static void test_name(tinytest::TestResult &);                                                                                              \
    _TT_APPEND_TEST(test_name, test_name);                                                                                                      \
    static void test_name(__attribute__((unused)) tinytest::TestResult &_tt_result)

/*
 * Create a subtest.
 * To run it, it has to be called from an other test. It is used to divide
 * a test to smaller independent parts.
 *
 * Arguments:
 *   - subtest_name
 *   - subtest_args...
 */
#define TINY_SUBTEST(...) _TT_CHOOSE_WRAPPER(_TT_TINY_SUBTEST, _TT_AT_LEAST_1_ARG(__VA_ARGS__), __VA_ARGS__)

/*
 * Run a subtest.
 * It can be used only in a test body.
 *
 * Arguments:
 *   - subtest_name
 *   - subtest_args...
 */
#define TINY_RUN_SUBTEST(...) _TT_CHOOSE_WRAPPER(_TT_TINY_RUN_SUBTEST, _TT_AT_LEAST_1_ARG(__VA_ARGS__), __VA_ARGS__)

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
    static void ptest_name(__attribute__((unused)) tinytest::TestResult &_tt_result, __VA_ARGS__)

/*
 * Create a test that is an instance of the given ptest.
 *
 * Arguments:
 *   - ptest_name
 *   - ptest_args...
 */
#define TINY_PTEST_INSTANCE(ptest_name, ...)                                                                                                    \
    static void _TT_CONCAT(_tt_ptest_instance_##ptest_name, __LINE__)(tinytest::TestResult &);                                                  \
    _TT_APPEND_TEST(ptest_name, _TT_CONCAT(_tt_ptest_instance_##ptest_name, __LINE__), _tt_ptest_args_format_##ptest_name, __VA_ARGS__);        \
    static void _TT_CONCAT(_tt_ptest_instance_##ptest_name, __LINE__)(tinytest::TestResult & result)                                            \
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
        _tt_result.passed = false;                                                                                                              \
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
        ++_tt_result.checks;                                                                                                                    \
        if ((expected) != (actual))                                                                                                             \
        {                                                                                                                                       \
            TINY_FAIL("values are different (expected = %d, actual = %d)", (expected), (actual));                                               \
            ++_tt_result.failed_checks;                                                                                                         \
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
        ++_tt_result.checks;                                                                                                                    \
        if (TINY_TEST_STRCMP(expected, actual) != 0)                                                                                            \
        {                                                                                                                                       \
            TINY_FAIL("values are different (expected = '%s', actual = '%s')", (expected), (actual));                                           \
            ++_tt_result.failed_checks;                                                                                                         \
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
        ++_tt_result.checks;                                                                                                                    \
        if (_TT_FABS((expected) - (actual)) > (epsilon))                                                                                        \
        {                                                                                                                                       \
            TINY_FAIL("values differ by more then %f (expected = %f, actual = %f)", (epsilon), (expected), (actual));                           \
            ++_tt_result.failed_checks;                                                                                                         \
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
        ++_tt_result.checks;                                                                                                                    \
        bool failed = false;                                                                                                                    \
        for (unsigned i = 0; i < (unsigned) (elements); ++i)                                                                                    \
            if ((expected)[i] != (actual)[i])                                                                                                   \
            {                                                                                                                                   \
                TINY_FAIL("memories differ at %u-th position (expected = %d, actual = %d)", i, (expected)[i], (actual)[i]);                     \
                failed = true;                                                                                                                  \
            }                                                                                                                                   \
        if (failed)                                                                                                                             \
            ++_tt_result.failed_checks;                                                                                                         \
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
        ++_tt_result.checks;                                                                                                                    \
        bool failed = false;                                                                                                                    \
        for (unsigned i = 0; i < (unsigned) (elements); ++i)                                                                                    \
            if (_TT_FABS((expected)[i] - (actual)[i]) > (epsilon))                                                                              \
            {                                                                                                                                   \
                TINY_FAIL("memories differ at %u-th position by more then %f (expected = %f, actual = %f)", i, (epsilon), (expected)[i],        \
                          (actual)[i]);                                                                                                         \
                failed = true;                                                                                                                  \
            }                                                                                                                                   \
        if (failed)                                                                                                                             \
            ++_tt_result.failed_checks;                                                                                                         \
    } while (false)

/*
 * Run test.
 *
 * Arguments:
 *   - test_name        - test name
 */
#define TINY_TEST_RUN_TEST(test_name) tinytest::run_test(_tt_test_##test_name)

/*
 * Run all tests in the test queue.
 */
#define TINY_TEST_RUN_ALL() tinytest::run_all_tests()

//------------------------------------------------------------------------------
//
// IMPLEMENTATION DETAILS
//
// It's not important for you if you only want to use TinyTest.
//
//------------------------------------------------------------------------------

// Floating point absolute value
#define _TT_FABS(a) ((a) < 0 ? -(a) : (a))

// Workaround for unsupported optional arguments in variadic macros. Works if
// number of arguments passed in __VA_ARGS__ is not greater then 100.
#define _TT_AT_LEAST_1_ARG_INNER(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, \
                                 _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46,  \
                                 _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, _65, _66, _67, _68,  \
                                 _69, _70, _71, _72, _73, _74, _75, _76, _77, _78, _79, _80, _81, _82, _83, _84, _85, _86, _87, _88, _89, _90,  \
                                 _91, _92, _93, _94, _95, _96, _97, _98, _99, _100, ARG, ...)                                                   \
    ARG

#define _TT_AT_LEAST_1_ARG(...)                                                                                                                 \
    _TT_AT_LEAST_1_ARG_INNER(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, \
                             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  \
                             1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0)

#define _TT_CHOOSE_WRAPPER_INNER(name, more_then_one, ...) name##_##more_then_one(__VA_ARGS__)

#define _TT_CHOOSE_WRAPPER(name, version, ...) _TT_CHOOSE_WRAPPER_INNER(name, version, __VA_ARGS__)

#define _TT_CONCAT_INNER(a, b) a##b
#define _TT_CONCAT(a, b)       _TT_CONCAT_INNER(a, b)

#define _TT_TINY_LOG_0(color, format)      TINY_TEST_PRINTF("[      ] " TINY_COLOR(color, "Line #%d: " format "\n"), __LINE__)
#define _TT_TINY_LOG_1(color, format, ...) TINY_TEST_PRINTF("[      ] " TINY_COLOR(color, "Line #%d: " format "\n"), __LINE__, __VA_ARGS__)

#define _TT_TINY_SUBTEST_0(subtest_name)      static void subtest_name(__attribute__((unused)) tinytest::TestResult &_tt_result)
#define _TT_TINY_SUBTEST_1(subtest_name, ...) static void subtest_name(__attribute__((unused)) tinytest::TestResult &_tt_result, __VA_ARGS__)

#define _TT_TINY_RUN_SUBTEST_0(subtest_name)      subtest_name(_tt_result)
#define _TT_TINY_RUN_SUBTEST_1(subtest_name, ...) subtest_name(_tt_result, __VA_ARGS__)

#define _TT_APPEND_TEST_0(test_name, test_body)                                                                                                 \
    static void _tt_test_##test_body(tinytest::TestResult &);                                                                                   \
    static tinytest::TestAppender _tt_appender_##test_body(_tt_test_##test_body);                                                               \
    static void _tt_test_##test_body(tinytest::TestResult &result)                                                                              \
    {                                                                                                                                           \
        TINY_TEST_PRINTF("[ TEST ] " #test_name " -- " __FILE__ ":%d\n", __LINE__);                                                             \
        test_body(result);                                                                                                                      \
        if (result.passed)                                                                                                                      \
            TINY_TEST_PRINTF("[------] " TINY_COLOR(TINY_GREEN, "Passed (%u/%u)\n"), result.checks, result.checks);                             \
        else                                                                                                                                    \
            TINY_TEST_PRINTF("[------] " TINY_COLOR(TINY_RED, "Failed (%u/%u)\n"), result.failed_checks, result.checks);                        \
    }

#define _TT_APPEND_TEST_1(test_name, test_body, test_args_format, ...)                                                                          \
    static void _tt_test_##test_body(tinytest::TestResult &);                                                                                   \
    static tinytest::TestAppender _tt_appender_##test_body(_tt_test_##test_body);                                                               \
    static void _tt_test_##test_body(tinytest::TestResult &result)                                                                              \
    {                                                                                                                                           \
        TINY_TEST_PRINTF("[ TEST ] " #test_name "%c", '(');                                                                                     \
        TINY_TEST_PRINTF(test_args_format, __VA_ARGS__);                                                                                        \
        TINY_TEST_PRINTF(") -- " __FILE__ ":%d\n", __LINE__);                                                                                   \
        test_body(result);                                                                                                                      \
        if (result.passed)                                                                                                                      \
            TINY_TEST_PRINTF("[------] " TINY_COLOR(TINY_GREEN, "Passed (%u/%u)\n"), result.checks, result.checks);                             \
        else                                                                                                                                    \
            TINY_TEST_PRINTF("[------] " TINY_COLOR(TINY_RED, "Failed (%u/%u)\n"), result.failed_checks, result.checks);                        \
    }

#define _TT_APPEND_TEST(test_name, ...) _TT_CHOOSE_WRAPPER(_TT_APPEND_TEST, _TT_AT_LEAST_1_ARG(__VA_ARGS__), test_name, __VA_ARGS__)

struct tinytest
{
    struct TestResult
    {
        bool passed;
        unsigned checks;
        unsigned failed_checks;
    };

    typedef void (*TestBody)(TestResult &);

    struct TestAppender
    {
        TestAppender(TestBody test_body)
        {
            *tinytest::all_tests_it++ = test_body;
        }
    };

    static TestResult run_test(TestBody body)
    {
        TestResult result = { true, 0, 0 };
        body(result);
        return result;
    }

    static bool run_all_tests()
    {
        TINY_TEST_PRINTF("================================================================================\n" //
                         TINY_TEST_NAME " v" TINY_TEST_VERSION "\n"                                           //
                         "================================================================================%c",
                         '\n');

        unsigned passed = 0;
        unsigned failed = 0;
        unsigned total_checks = 0;
        unsigned total_failed_checks = 0;
        for (const TestBody *it = all_tests; it != all_tests_it; ++it)
        {
            if (it != all_tests)
                TINY_TEST_PRINTF("%c", '\n');
            TestResult result = { true, 0, 0 };
            (*it)(result);

            total_checks += result.checks;
            total_failed_checks += result.failed_checks;

            if (result.passed)
                ++passed;
            else
                ++failed;
        }

        TINY_TEST_PRINTF("================================================================================\n"  //
                         TINY_COLOR(TINY_GREEN, "Passed   %u (%u/%u)\n")                                       //
                         TINY_COLOR(TINY_RED, "Failed   %u (%u/%u)\n")                                         //
                         "================================================================================\n", //
                         passed,                                                                               //
                         total_checks - total_failed_checks,                                                   //
                         total_checks,                                                                         //
                         failed,                                                                               //
                         total_failed_checks,                                                                  //
                         total_checks                                                                          //
        );

        if (failed == 0)
            TINY_TEST_PRINTF(TINY_COLOR(TINY_GREEN, "All tests passed!\n%c"), '\n');
        else
            TINY_TEST_PRINTF(TINY_COLOR(TINY_RED, "%d %s failed!\n\n"), failed, (failed == 1 ? "test" : "tests"));

        return failed;
    }

  private:
    static TestBody all_tests[TINY_TEST_MAX_TESTS];
    static TestBody *all_tests_it;
};

inline tinytest::TestBody tinytest::all_tests[TINY_TEST_MAX_TESTS] = {};
inline tinytest::TestBody *tinytest::all_tests_it = tinytest::all_tests;

#endif
