// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/attributes.h"
#include "mos/stdio.h"
#include "mos/types.h"
#include "test_engine.h"

char buffer[2048] = { 0 };

int tst_printf(char *buffer, const char *format, ...);

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

TINY_TEST(simple_string)
{
    PRINTF_TEST("a", "a", );
    PRINTF_TEST("very long string", "very long string", );
    PRINTF_TEST(
        "d6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880",
        "d6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880", );
}

TINY_TEST(integer)
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

    // Precision
    PRINTF_TEST("123", "%.0d", 123);
    PRINTF_TEST("123", "%.1d", 123);
    PRINTF_TEST("123", "%.2d", 123);
    PRINTF_TEST("123", "%.3d", 123);
    PRINTF_TEST("0123", "%.4d", 123);
    PRINTF_TEST("00123", "%.5d", 123);
    PRINTF_TEST("000123", "%.6d", 123);
    PRINTF_TEST("0000123", "%.7d", 123);
    PRINTF_TEST("00000123", "%.8d", 123);

    // Precision with sign
    PRINTF_TEST("+123", "%+.0d", 123);
    PRINTF_TEST("+123", "%+.1d", 123);
    PRINTF_TEST("+123", "%+.2d", 123);
    PRINTF_TEST("+123", "%+.3d", 123);
    PRINTF_TEST("+0123", "%+.4d", 123);
    PRINTF_TEST("+00123", "%+.5d", 123);
    PRINTF_TEST("+000123", "%+.6d", 123);
    PRINTF_TEST("+0000123", "%+.7d", 123);
    PRINTF_TEST("+00000123", "%+.8d", 123);

    // Precision with zero padding
    PRINTF_TEST("123", "%0.0d", 123);
    PRINTF_TEST("123", "%0.1d", 123);
    PRINTF_TEST("123", "%0.2d", 123);
    PRINTF_TEST("123", "%0.3d", 123);
    PRINTF_TEST("0123", "%0.4d", 123);
    PRINTF_TEST("00123", "%0.5d", 123);
    PRINTF_TEST("000123", "%0.6d", 123);
    PRINTF_TEST("0000123", "%0.7d", 123);
    PRINTF_TEST("00000123", "%0.8d", 123);

    // Precision with zero padding and sign
    // ! "If a precision is given with a numeric conversion (d, i, o, u, x, and X), the 0 flag is ignored."
    PRINTF_TEST("+123", "%0+.0d", 123);
    PRINTF_TEST("+123", "%0+.1d", 123);
    PRINTF_TEST("+123", "%0+.2d", 123);
    PRINTF_TEST("+123", "%0+.3d", 123);
    PRINTF_TEST("+0123", "%0+.4d", 123);
    PRINTF_TEST("+00123", "%0+.5d", 123);
    PRINTF_TEST("+000123", "%0+.6d", 123);
    PRINTF_TEST("+0000123", "%0+.7d", 123);
    PRINTF_TEST("+00000123", "%0+.8d", 123);

    // Precision with space (sign placeholder)
    PRINTF_TEST(" 123", "% .0d", 123);
    PRINTF_TEST(" 123", "% .1d", 123);
    PRINTF_TEST(" 123", "% .2d", 123);
    PRINTF_TEST(" 123", "% .3d", 123);
    PRINTF_TEST(" 0123", "% .4d", 123);
    PRINTF_TEST(" 00123", "% .5d", 123);
    PRINTF_TEST(" 000123", "% .6d", 123);
    PRINTF_TEST(" 0000123", "% .7d", 123);
    PRINTF_TEST(" 00000123", "% .8d", 123);

    // Precision with width

    PRINTF_TEST("123", "%0.0d", 123);
    PRINTF_TEST("123", "%0.1d", 123);
    PRINTF_TEST("123", "%0.2d", 123);
    PRINTF_TEST("123", "%0.3d", 123);
    PRINTF_TEST("0123", "%0.4d", 123);
    PRINTF_TEST("00123", "%0.5d", 123);
    PRINTF_TEST("000123", "%0.6d", 123);
    PRINTF_TEST("0000123", "%0.7d", 123);
    PRINTF_TEST("00000123", "%0.8d", 123);

    PRINTF_TEST("123", "%1.0d", 123);
    PRINTF_TEST("123", "%1.1d", 123);
    PRINTF_TEST("123", "%1.2d", 123);
    PRINTF_TEST("123", "%1.3d", 123);
    PRINTF_TEST("0123", "%1.4d", 123);
    PRINTF_TEST("00123", "%1.5d", 123);
    PRINTF_TEST("000123", "%1.6d", 123);
    PRINTF_TEST("0000123", "%1.7d", 123);
    PRINTF_TEST("00000123", "%1.8d", 123);

    PRINTF_TEST("123", "%2.0d", 123);
    PRINTF_TEST("123", "%2.1d", 123);
    PRINTF_TEST("123", "%2.2d", 123);
    PRINTF_TEST("123", "%2.3d", 123);
    PRINTF_TEST("0123", "%2.4d", 123);
    PRINTF_TEST("00123", "%2.5d", 123);
    PRINTF_TEST("000123", "%2.6d", 123);
    PRINTF_TEST("0000123", "%2.7d", 123);
    PRINTF_TEST("00000123", "%2.8d", 123);

    PRINTF_TEST("123", "%3.0d", 123);
    PRINTF_TEST("123", "%3.1d", 123);
    PRINTF_TEST("123", "%3.2d", 123);
    PRINTF_TEST("123", "%3.3d", 123);
    PRINTF_TEST("0123", "%3.4d", 123);
    PRINTF_TEST("00123", "%3.5d", 123);
    PRINTF_TEST("000123", "%3.6d", 123);
    PRINTF_TEST("0000123", "%3.7d", 123);
    PRINTF_TEST("00000123", "%3.8d", 123);

    PRINTF_TEST(" 123", "%4.0d", 123);
    PRINTF_TEST(" 123", "%4.1d", 123);
    PRINTF_TEST(" 123", "%4.2d", 123);
    PRINTF_TEST(" 123", "%4.3d", 123);
    PRINTF_TEST("0123", "%4.4d", 123);
    PRINTF_TEST("00123", "%4.5d", 123);
    PRINTF_TEST("000123", "%4.6d", 123);
    PRINTF_TEST("0000123", "%4.7d", 123);
    PRINTF_TEST("00000123", "%4.8d", 123);
}

TINY_TEST_CASES test_cases[2] = {
    { "simple_string", simple_string },
    { "integer", integer },
};
