// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/attributes.h"
#include "mos/stdio.h"
#include "test_engine.h"
#include "tinytest.h"

static char buffer[2048] = { 0 };

int tst_printf(char *buffer, const char *format, ...) __attribute__((format(printf, 2, 3)));

#define PRINTF_TEST(expected, format, ...)                                                                                                      \
    tst_printf(buffer, format __VA_OPT__(, ) __VA_ARGS__);                                                                                      \
    MOS_TEST_CHECK_STRING(expected, buffer);

int tst_printf(char *buffer, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(buffer, 0, format, args);
    va_end(args);
    return ret;
}

MOS_TEST_CASE(percent_sign)
{
    MOS_WARNING_PUSH;
    MOS_WARNING_DISABLE("-Wformat");
    {
        PRINTF_TEST("%", "%%", );
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("", "%", ), "format string is empty");
    }
    MOS_WARNING_POP;
}

MOS_TEST_CASE(simple_string)
{
    PRINTF_TEST("a", "a", );
    PRINTF_TEST("very long string", "very long string", );
    PRINTF_TEST(
        "d6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880",
        "d6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880", );
}

MOS_TEST_CASE(integer_no_precision)
{
    PRINTF_TEST("-123", "%d", -123);
    PRINTF_TEST("0", "%d", 0);
    PRINTF_TEST("123", "%d", 123);

    // With sign and space
    // Negative numbers always have a sign.
    PRINTF_TEST("-123", "% d", -123);
    PRINTF_TEST("-123", "%+d", -123);

    // Positive numbers have a plus if a plus is specified, or a space if a space is specified.
    PRINTF_TEST("+123", "%+d", 123);
    PRINTF_TEST(" 123", "% d", 123);
    PRINTF_TEST("-123", "% d", -123);

    // zero is positive !!!!
    PRINTF_TEST("+0", "%+d", 0);
    PRINTF_TEST(" 0", "% d", 0);

    PRINTF_TEST("-0011", "%05i", -11);

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

    PRINTF_TEST("-123", "%3d", -123);
    PRINTF_TEST("-123", "%4d", -123);
    PRINTF_TEST(" -123", "%5d", -123);
    PRINTF_TEST("  -123", "%6d", -123);
    PRINTF_TEST("   -123", "%7d", -123);

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

MOS_TEST_CASE(integer_with_precision)
{
    PRINTF_TEST("-00011", "%.5i", -11);

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

    // ! "If a precision is given with a numeric conversion (d, i, o, u, x, and X), the 0 flag is ignored."
    MOS_WARNING_PUSH
    MOS_WARNING_DISABLE("-Wformat");
    {

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
        PRINTF_TEST("+123", "%0+.0d", 123);
        PRINTF_TEST("+123", "%0+.1d", 123);
        PRINTF_TEST("+123", "%0+.2d", 123);
        PRINTF_TEST("+123", "%0+.3d", 123);
        PRINTF_TEST("+0123", "%0+.4d", 123);
        PRINTF_TEST("+00123", "%0+.5d", 123);
        PRINTF_TEST("+000123", "%0+.6d", 123);
        PRINTF_TEST("+0000123", "%0+.7d", 123);
        PRINTF_TEST("+00000123", "%0+.8d", 123);

        PRINTF_TEST("+123", "%+0.0d", 123);
        PRINTF_TEST("+123", "%+0.1d", 123);
        PRINTF_TEST("+123", "%+0.2d", 123);
        PRINTF_TEST("+123", "%+0.3d", 123);
        PRINTF_TEST("+0123", "%+0.4d", 123);
        PRINTF_TEST("+00123", "%+0.5d", 123);
        PRINTF_TEST("+000123", "%+0.6d", 123);
        PRINTF_TEST("+0000123", "%+0.7d", 123);
        PRINTF_TEST("+00000123", "%+0.8d", 123);
    }
    MOS_WARNING_POP

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

    PRINTF_TEST("  123", "%5.0d", 123);
    PRINTF_TEST("  123", "%5.1d", 123);
    PRINTF_TEST("  123", "%5.2d", 123);
    PRINTF_TEST("  123", "%5.3d", 123);
    PRINTF_TEST(" 0123", "%5.4d", 123);
    PRINTF_TEST("00123", "%5.5d", 123);
    PRINTF_TEST("000123", "%5.6d", 123);
    PRINTF_TEST("0000123", "%5.7d", 123);
    PRINTF_TEST("00000123", "%5.8d", 123);

    PRINTF_TEST("   123", "%6.0d", 123);
    PRINTF_TEST("   123", "%6.1d", 123);
    PRINTF_TEST("   123", "%6.2d", 123);
    PRINTF_TEST("   123", "%6.3d", 123);
    PRINTF_TEST("  0123", "%6.4d", 123);
    PRINTF_TEST(" 00123", "%6.5d", 123);
    PRINTF_TEST("000123", "%6.6d", 123);
    PRINTF_TEST("0000123", "%6.7d", 123);
    PRINTF_TEST("00000123", "%6.8d", 123);

    PRINTF_TEST("    123", "%7.0d", 123);
    PRINTF_TEST("    123", "%7.1d", 123);
    PRINTF_TEST("    123", "%7.2d", 123);
    PRINTF_TEST("    123", "%7.3d", 123);
    PRINTF_TEST("   0123", "%7.4d", 123);
    PRINTF_TEST("  00123", "%7.5d", 123);
    PRINTF_TEST(" 000123", "%7.6d", 123);
    PRINTF_TEST("0000123", "%7.7d", 123);
    PRINTF_TEST("00000123", "%7.8d", 123);
}

MOS_TEST_CASE(integer_left_justified)
{
    MOS_WARNING_PUSH
    MOS_WARNING_DISABLE("-Wformat")
    {
        // Left-justified with zero padding (ignored, warning expected)
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("123", "%0-d", 123), "expected a warning about zero-padding");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-123", "%0-d", -123), "expected a warning about zero-padding");
    }
    MOS_WARNING_POP

    // Left-justified with sign
    PRINTF_TEST("+123", "%+-d", 123);

    // Left-justified with space (sign placeholder)
    PRINTF_TEST(" 123", "% -d", 123);

    // Left-justified with width
    PRINTF_TEST("123", "%-1d", 123);
    PRINTF_TEST("123", "%-2d", 123);
    PRINTF_TEST("123", "%-3d", 123);
    PRINTF_TEST("123 ", "%-4d", 123);
    PRINTF_TEST("123  ", "%-5d", 123);
    PRINTF_TEST("123   ", "%-6d", 123);
    PRINTF_TEST("123    ", "%-7d", 123);

    // Left-justified with width and sign
    PRINTF_TEST("+123", "%-+1d", 123);
    PRINTF_TEST("+123", "%-+2d", 123);
    PRINTF_TEST("+123", "%-+3d", 123);
    PRINTF_TEST("+123", "%-+4d", 123);
    PRINTF_TEST("+123 ", "%-+5d", 123);
    PRINTF_TEST("+123  ", "%-+6d", 123);
    PRINTF_TEST("+123   ", "%-+7d", 123);
    PRINTF_TEST("+123    ", "%-+8d", 123);

    // Left-justified with width and space (sign placeholder)
    PRINTF_TEST(" 123", "% -1d", 123);
    PRINTF_TEST(" 123", "% -2d", 123);
    PRINTF_TEST(" 123", "% -3d", 123);
    PRINTF_TEST(" 123", "% -4d", 123);
    PRINTF_TEST(" 123 ", "% -5d", 123);
    PRINTF_TEST(" 123  ", "% -6d", 123);
    PRINTF_TEST(" 123   ", "% -7d", 123);
    PRINTF_TEST(" 123    ", "% -8d", 123);

    // Left-justified with precision
    PRINTF_TEST("123", "%-.0d", 123);
    PRINTF_TEST("123", "%-.1d", 123);
    PRINTF_TEST("123", "%-.2d", 123);
    PRINTF_TEST("123", "%-.3d", 123);
    PRINTF_TEST("0123", "%-.4d", 123);
    PRINTF_TEST("00123", "%-.5d", 123);
    PRINTF_TEST("000123", "%-.6d", 123);
    PRINTF_TEST("0000123", "%-.7d", 123);
    PRINTF_TEST("00000123", "%-.8d", 123);

    // Left-justified with precision and width
    PRINTF_TEST("123", "%-1.0d", 123);
    PRINTF_TEST("123", "%-1.1d", 123);
    PRINTF_TEST("123", "%-1.2d", 123);
    PRINTF_TEST("123", "%-1.3d", 123);
    PRINTF_TEST("0123", "%-1.4d", 123);
    PRINTF_TEST("00123", "%-1.5d", 123);
    PRINTF_TEST("000123", "%-1.6d", 123);
    PRINTF_TEST("0000123", "%-1.7d", 123);
    PRINTF_TEST("00000123", "%-1.8d", 123);

    PRINTF_TEST("123", "%-2.0d", 123);
    PRINTF_TEST("123", "%-2.1d", 123);
    PRINTF_TEST("123", "%-2.2d", 123);
    PRINTF_TEST("123", "%-2.3d", 123);
    PRINTF_TEST("0123", "%-2.4d", 123);
    PRINTF_TEST("00123", "%-2.5d", 123);
    PRINTF_TEST("000123", "%-2.6d", 123);
    PRINTF_TEST("0000123", "%-2.7d", 123);
    PRINTF_TEST("00000123", "%-2.8d", 123);

    PRINTF_TEST("123", "%-3.0d", 123);
    PRINTF_TEST("123", "%-3.1d", 123);
    PRINTF_TEST("123", "%-3.2d", 123);
    PRINTF_TEST("123", "%-3.3d", 123);
    PRINTF_TEST("0123", "%-3.4d", 123);
    PRINTF_TEST("00123", "%-3.5d", 123);
    PRINTF_TEST("000123", "%-3.6d", 123);
    PRINTF_TEST("0000123", "%-3.7d", 123);
    PRINTF_TEST("00000123", "%-3.8d", 123);

    PRINTF_TEST("123 ", "%-4.0d", 123);
    PRINTF_TEST("123 ", "%-4.1d", 123);
    PRINTF_TEST("123 ", "%-4.2d", 123);
    PRINTF_TEST("123 ", "%-4.3d", 123);
    PRINTF_TEST("0123", "%-4.4d", 123);
    PRINTF_TEST("00123", "%-4.5d", 123);
    PRINTF_TEST("000123", "%-4.6d", 123);
    PRINTF_TEST("0000123", "%-4.7d", 123);
    PRINTF_TEST("00000123", "%-4.8d", 123);

    PRINTF_TEST("123  ", "%-5.0d", 123);
    PRINTF_TEST("123  ", "%-5.1d", 123);
    PRINTF_TEST("123  ", "%-5.2d", 123);
    PRINTF_TEST("123  ", "%-5.3d", 123);
    PRINTF_TEST("0123 ", "%-5.4d", 123);
    PRINTF_TEST("00123", "%-5.5d", 123);
    PRINTF_TEST("000123", "%-5.6d", 123);
    PRINTF_TEST("0000123", "%-5.7d", 123);
    PRINTF_TEST("00000123", "%-5.8d", 123);

    PRINTF_TEST("123   ", "%-6.0d", 123);
    PRINTF_TEST("123   ", "%-6.1d", 123);
    PRINTF_TEST("123   ", "%-6.2d", 123);
    PRINTF_TEST("123   ", "%-6.3d", 123);
    PRINTF_TEST("0123  ", "%-6.4d", 123);
    PRINTF_TEST("00123 ", "%-6.5d", 123);
    PRINTF_TEST("000123", "%-6.6d", 123);
    PRINTF_TEST("0000123", "%-6.7d", 123);
    PRINTF_TEST("00000123", "%-6.8d", 123);
}

MOS_TEST_CASE(integer_extreme_case)
{
    PRINTF_TEST("2147483647", "%d", 2147483647);
    PRINTF_TEST("-2147483648", "%d", -2147483647 - 1);
    PRINTF_TEST("0", "%d", +0);
    PRINTF_TEST("0", "%d", 0);
    PRINTF_TEST("0", "%d", -0);
}
