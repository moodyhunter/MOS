#include "unittest.h"

#include <iostream>
#include <stdarg.h>
#include <string.h>

char buffer[2048] = { 0 };

#define PRINTF_TEST(expected, format, ...)                                                                                                      \
    {                                                                                                                                           \
        tst_printf(buffer, format __VA_OPT__(, ) __VA_ARGS__);                                                                                  \
        TINY_CHECK_STRING(expected, buffer);                                                                                                    \
    }

int tst_printf(char *buffer, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, 0, format, args);
    va_end(args);
    return ret;
}

TINY_SUBTEST(simple_string)
{
    PRINTF_TEST("a", "a");
    PRINTF_TEST("very long string", "very long string");
    PRINTF_TEST(
        "d6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880",
        "d6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880");
}

TINY_SUBTEST(integer)
{
    PRINTF_TEST("-123", "%d", -123);
    PRINTF_TEST("0", "%d", 0);
    PRINTF_TEST("123", "%d", 123);

    // Extremes
    PRINTF_TEST("-2147483648", "%d", -2147483648);
    PRINTF_TEST("2147483647", "%d", 2147483647);

    // With sign and space
    // Negative numbers always have a sign.
    PRINTF_TEST("-123", "% d", -123);
    PRINTF_TEST("-123", "%+d", -123);

    // Positive numbers have a plus if a plus is specified, or a space if a space is specified.
    PRINTF_TEST("+123", "%+d", 123);
    PRINTF_TEST(" 123", "% d", 123);

    // zero is positive !!!!
    PRINTF_TEST("+0", "%+d", 0);
    PRINTF_TEST(" 0", "% d", 0);

    // Minimum field width
    PRINTF_TEST("123", "%3d", 123);
    PRINTF_TEST("  123", "%5d", 123);
    PRINTF_TEST("   123", "%6d", 123);
    PRINTF_TEST("    123", "%7d", 123);

    // Minimum field width with sign
    PRINTF_TEST("+123", "%+3d", 123);
    PRINTF_TEST(" +123", "%+5d", 123);
    PRINTF_TEST("  +123", "%+6d", 123);
    PRINTF_TEST("   +123", "%+7d", 123);

    // Minimum field width with zero padding
    PRINTF_TEST("123", "%03d", 123);
    PRINTF_TEST("00123", "%05d", 123);
    PRINTF_TEST("000123", "%06d", 123);
    PRINTF_TEST("0000123", "%07d", 123);

    // Minimum field width with zero padding and sign
    PRINTF_TEST("+123", "%+03d", 123);
    PRINTF_TEST("+0123", "%+05d", 123);
    PRINTF_TEST("+00123", "%+06d", 123);
    PRINTF_TEST("+000123", "%+07d", 123);

    // Minimum field width with zero padding and sign
    PRINTF_TEST("-123", "%03d", -123);
    PRINTF_TEST("-0123", "%05d", -123);
    PRINTF_TEST("-00123", "%06d", -123);
    PRINTF_TEST("-000123", "%07d", -123);
}

TINY_TEST(print_string)
{
    TINY_RUN_SUBTEST(simple_string);
}

TINY_TEST(print_int)
{
    TINY_RUN_SUBTEST(integer);
}

extern "C"
{
    void _kwarn_impl(const char *msg, const char *func, const char *file, const char *line)
    {
        TINY_LOG(TINY_YELLOW, "%s:%s:%s:%s", msg, func, file, line);
    }

    void _kpanic_impl(const char *msg, const char *func, const char *file, const char *line)
    {
        TINY_LOG(TINY_RED, "KERNEL PANIC: %s, in function '%s' from file %s:%s", msg, func, file, line);
        std::exit(1);
    }
}

int main()
{
    return TINY_TEST_RUN_ALL();
}
