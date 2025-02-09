// SPDX-License-Identifier: GPL-3.0-or-later

#include "test_engine_impl.h"

#include <mos_stdio.hpp>

static char buffer[2048] = { 0 };

MOS_TEST_DEFINE_CONDITION(printf_tests_enable_posix, "POSIX exts") = false;
MOS_TEST_DEFINE_CONDITION(printf_tests_enable_floats, "floating points") = false;
MOS_TEST_DEFINE_CONDITION(printf_tests_enable_egp, "e, g, p tests") = false;
MOS_TEST_DEFINE_CONDITION(printf_tests_enable_oxX, "o, x, X tests") = true;

#define PRINTF_TEST(expected, format, ...)                                                                                                                               \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (MOS_TEST_CURRENT_TEST_SKIPPED)                                                                                                                               \
        {                                                                                                                                                                \
            MOS_TEST_SKIP();                                                                                                                                             \
            break;                                                                                                                                                       \
        }                                                                                                                                                                \
        sprintf(buffer, format __VA_OPT__(, ) __VA_ARGS__);                                                                                                              \
        MOS_TEST_CHECK_STRING(buffer, expected);                                                                                                                         \
    } while (0)

MOS_TEST_CASE(percent_sign)
{
    MOS_WARNING_PUSH;
    MOS_WARNING_DISABLE("-Wformat");
    MOS_WARNING_DISABLE("-Wformat-zero-length");
    {
        PRINTF_TEST("", "", );
        PRINTF_TEST("%", "%%", );
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("", "%", ), "format string is incomplete");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("incomplete ", "incomplete %", ), "incomplete format specifier");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("incomplete 100", "incomplete %d%", 100), "incomplete format specifier");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("incomplete 'abcde' %", "incomplete '%s' %%%", "abcde"), "incomplete format specifier");
    }
    MOS_WARNING_POP;
}

MOS_TEST_CASE(simple_string)
{
    PRINTF_TEST("a", "a", );
    PRINTF_TEST("very long string", "very long string", );
    PRINTF_TEST("d6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880-464eeed9541cd6c40101-371d-473e-8880",
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

// https://github.com/BartMassey/printf-tests/blob/master/drivers/c/testcases.c
// Licensed under the GPLv2 or later.
MOS_WARNING_PUSH
MOS_WARNING_DISABLE("-Wformat")
MOS_WARNING_DISABLE("-Wformat-extra-args")
MOS_TEST_CASE(printf_tests_github)
{
    PRINTF_TEST("     00004", "%10.5d", 4);
    PRINTF_TEST(" 42", "% d", 42);
    PRINTF_TEST("-42", "% d", -42);
    PRINTF_TEST("   42", "% 5d", 42);
    PRINTF_TEST("  -42", "% 5d", -42);
    PRINTF_TEST("             42", "% 15d", 42);
    PRINTF_TEST("            -42", "% 15d", -42);
    PRINTF_TEST("+42", "%+d", 42);
    PRINTF_TEST("-42", "%+d", -42);
    PRINTF_TEST("  +42", "%+5d", 42);
    PRINTF_TEST("  -42", "%+5d", -42);
    PRINTF_TEST("1234", "%3d", 1234);
    PRINTF_TEST("            +42", "%+15d", 42);
    PRINTF_TEST("            -42", "%+15d", -42);
    PRINTF_TEST("42", "%0d", 42);
    PRINTF_TEST("-42", "%0d", -42);
    PRINTF_TEST("00042", "%05d", 42);
    PRINTF_TEST("-0042", "%05d", -42);
    PRINTF_TEST("000000000000042", "%015d", 42);
    PRINTF_TEST("-00000000000042", "%015d", -42);
    PRINTF_TEST("42", "%-d", 42);
    PRINTF_TEST("-42", "%-d", -42);
    PRINTF_TEST("2", "%-1d", 2);
    PRINTF_TEST("42   ", "%-5d", 42);
    PRINTF_TEST("-42  ", "%-5d", -42);
    PRINTF_TEST("42             ", "%-15d", 42);
    PRINTF_TEST("-42            ", "%-15d", -42);
    PRINTF_TEST("10", "%d", 10);
    PRINTF_TEST("+10+", "+%d+", 10);
    PRINTF_TEST("1024", "%d", 1024);
    PRINTF_TEST("-1024", "%d", -1024);
    PRINTF_TEST(" 0000000000000000000000000000000000000001", "% .40d", 1);

    PRINTF_TEST("a", "%c", 'a');
    PRINTF_TEST(" ", "%c", 32);
    PRINTF_TEST("$", "%c", 36);

    PRINTF_TEST("  a", "%3c", 'a');

    PRINTF_TEST("1024", "%i", 1024);
    PRINTF_TEST("-1024", "%i", -1024);
    PRINTF_TEST("-1", "%-i", -1);
    PRINTF_TEST("1", "%-i", 1);
    PRINTF_TEST("+1", "%+i", 1);
    PRINTF_TEST("1024", "%u", 1024);
    PRINTF_TEST("4294967295", "%u", -1);

    PRINTF_TEST("+hello+", "+%s+", "hello");
    PRINTF_TEST("%%%%", "%s", "%%%%");
    PRINTF_TEST("hello", "hello", );
    PRINTF_TEST("Hallo heimur", "Hallo heimur", );
    PRINTF_TEST("Hallo heimur", "%s", "Hallo heimur");
    PRINTF_TEST("foo", "%.3s", "foobar");
    PRINTF_TEST(" foo", "%*s", 4, "foo");

    // lld
    PRINTF_TEST("    +100", "%+8lld", 100LL);
    PRINTF_TEST("+00000100", "%+.8lld", 100LL);
    PRINTF_TEST(" +00000100", "%+10.8lld", 100LL);
    PRINTF_TEST("-00100", "%-1.5lld", -100LL);
    PRINTF_TEST("  100", "%5lld", 100LL);
    PRINTF_TEST(" -100", "%5lld", -100LL);
    PRINTF_TEST("100  ", "%-5lld", 100LL);
    PRINTF_TEST("-100 ", "%-5lld", -100LL);
    PRINTF_TEST("00100", "%-.5lld", 100LL);
    PRINTF_TEST("-00100", "%-.5lld", -100LL);
    PRINTF_TEST("00100   ", "%-8.5lld", 100LL);
    PRINTF_TEST("-00100  ", "%-8.5lld", -100LL);
    PRINTF_TEST("00100", "%05lld", 100LL);
    PRINTF_TEST("-0100", "%05lld", -100LL);
    PRINTF_TEST(" 100", "% lld", 100LL);
    PRINTF_TEST("-100", "% lld", -100LL);
    PRINTF_TEST("  100", "% 5lld", 100LL);
    PRINTF_TEST(" -100", "% 5lld", -100LL);
    PRINTF_TEST(" 00100", "% .5lld", 100LL);
    PRINTF_TEST("-00100", "% .5lld", -100LL);
    PRINTF_TEST("   00100", "% 8.5lld", 100LL);
    PRINTF_TEST("  -00100", "% 8.5lld", -100LL);
    PRINTF_TEST("", "%.0lld", 0LL);

    PRINTF_TEST("0000000000000000000000000000000000000001", "%.40lld", 1LL);
    PRINTF_TEST(" 0000000000000000000000000000000000000001", "% .40lld", 1LL);

    // star
    PRINTF_TEST("               Hallo", "%*s", 20, "Hallo");
    PRINTF_TEST("                1024", "%*d", 20, 1024);
    PRINTF_TEST("               -1024", "%*d", 20, -1024);
    PRINTF_TEST("                1024", "%*i", 20, 1024);
    PRINTF_TEST("               -1024", "%*i", 20, -1024);
    PRINTF_TEST("                1024", "%*u", 20, 1024);
    PRINTF_TEST("          4294966272", "%*u", 20, 4294966272U);
    PRINTF_TEST("                   x", "%*c", 20, 'x');

    PRINTF_TEST("hi x\\n", "%*sx\\n", -3, "hi");

    PRINTF_TEST("f", "%.1s", "foo");
    PRINTF_TEST("f", "%.*s", 1, "foo");
    PRINTF_TEST("foo  ", "%*s", -5, "foo");
    PRINTF_TEST("%0", "%%0", );
    PRINTF_TEST("4294966272", "%u", 4294966272U);
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("%H", "%H", -1), "unknown conversion specifier");
    PRINTF_TEST("x", "%c", 'x');
    PRINTF_TEST("%", "%%", );
    PRINTF_TEST("+1024", "%+d", 1024);
    PRINTF_TEST("-1024", "%+d", -1024);
    PRINTF_TEST("+1024", "%+i", 1024);
    PRINTF_TEST("-1024", "%+i", -1024);
    PRINTF_TEST(" 1024", "% d", 1024);
    PRINTF_TEST("-1024", "% d", -1024);
    PRINTF_TEST(" 1024", "% i", 1024);
    PRINTF_TEST("-1024", "% i", -1024);
    PRINTF_TEST("Hallo heimur", "%1s", "Hallo heimur");
    PRINTF_TEST("1024", "%1d", 1024);
    PRINTF_TEST("-1024", "%1d", -1024);
    PRINTF_TEST("1024", "%1i", 1024);
    PRINTF_TEST("-1024", "%1i", -1024);
    PRINTF_TEST("1024", "%1u", 1024);
    PRINTF_TEST("4294966272", "%1u", 4294966272U);
    PRINTF_TEST("x", "%1c", 'x');
    PRINTF_TEST("               Hallo", "%20s", "Hallo");
    PRINTF_TEST("                1024", "%20d", 1024);
    PRINTF_TEST("               -1024", "%20d", -1024);
    PRINTF_TEST("                1024", "%20i", 1024);
    PRINTF_TEST("               -1024", "%20i", -1024);
    PRINTF_TEST("                1024", "%20u", 1024);
    PRINTF_TEST("          4294966272", "%20u", 4294966272U);
    PRINTF_TEST("                   x", "%20c", 'x');
    PRINTF_TEST("Hallo               ", "%-20s", "Hallo");
    PRINTF_TEST("1024                ", "%-20d", 1024);
    PRINTF_TEST("-1024               ", "%-20d", -1024);
    PRINTF_TEST("1024                ", "%-20i", 1024);
    PRINTF_TEST("-1024               ", "%-20i", -1024);
    PRINTF_TEST("1024                ", "%-20u", 1024);
    PRINTF_TEST("4294966272          ", "%-20u", 4294966272U);
    PRINTF_TEST("x                   ", "%-20c", 'x');
    PRINTF_TEST("00000000000000001024", "%020d", 1024);
    PRINTF_TEST("-0000000000000001024", "%020d", -1024);
    PRINTF_TEST("00000000000000001024", "%020i", 1024);
    PRINTF_TEST("-0000000000000001024", "%020i", -1024);
    PRINTF_TEST("00000000000000001024", "%020u", 1024);
    PRINTF_TEST("00000000004294966272", "%020u", 4294966272U);
    PRINTF_TEST("Hallo heimur", "%.20s", "Hallo heimur");
    PRINTF_TEST("00000000000000001024", "%.20d", 1024);
    PRINTF_TEST("-00000000000000001024", "%.20d", -1024);
    PRINTF_TEST("00000000000000001024", "%.20i", 1024);
    PRINTF_TEST("-00000000000000001024", "%.20i", -1024);
    PRINTF_TEST("00000000000000001024", "%.20u", 1024);
    PRINTF_TEST("00000000004294966272", "%.20u", 4294966272U);
    PRINTF_TEST("               Hallo", "%20.5s", "Hallo heimur");
    PRINTF_TEST("               01024", "%20.5d", 1024);
    PRINTF_TEST("              -01024", "%20.5d", -1024);
    PRINTF_TEST("               01024", "%20.5i", 1024);
    PRINTF_TEST("              -01024", "%20.5i", -1024);
    PRINTF_TEST("               01024", "%20.5u", 1024);
    PRINTF_TEST("          4294966272", "%20.5u", 4294966272U);
    PRINTF_TEST("               01024", "%020.5d", 1024);
    PRINTF_TEST("              -01024", "%020.5d", -1024);
    PRINTF_TEST("               01024", "%020.5i", 1024);
    PRINTF_TEST("              -01024", "%020.5i", -1024);
    PRINTF_TEST("               01024", "%020.5u", 1024);
    PRINTF_TEST("          4294966272", "%020.5u", 4294966272U);
    PRINTF_TEST("", "%.0s", "Hallo heimur");
    PRINTF_TEST("                    ", "%20.0s", "Hallo heimur");
    PRINTF_TEST("", "%.s", "Hallo heimur");
    PRINTF_TEST("                    ", "%20.s", "Hallo heimur");
    PRINTF_TEST("                1024", "%20.0d", 1024);
    PRINTF_TEST("               -1024", "%20.d", -1024);
    PRINTF_TEST("                    ", "%20.d", 0);
    PRINTF_TEST("                1024", "%20.0i", 1024);
    PRINTF_TEST("               -1024", "%20.i", -1024);
    PRINTF_TEST("                    ", "%20.i", 0);
    PRINTF_TEST("                1024", "%20.u", 1024);
    PRINTF_TEST("          4294966272", "%20.0u", 4294966272U);
    PRINTF_TEST("                    ", "%20.u", 0U);

    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("%w", "%w", -1), "unknown format specifier 'w'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("%b", "%b", ), "unknown format specifier 'b'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("%(foo", "%(foo", ), "unknown format specifier");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("%_1lld", "%_1lld", 100LL), "unknown format specifier '_'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("42", "%-0d", 42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-42", "%-0d", -42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("42   ", "%-05d", 42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-42  ", "%-05d", -42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("42             ", "%-015d", 42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-42            ", "%-015d", -42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("42", "%0-d", 42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-42", "%0-d", -42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("42   ", "%0-5d", 42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-42  ", "%0-5d", -42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("42             ", "%0-15d", 42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-42            ", "%0-15d", -42), "0 ignored by '-'");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("  -0000000000000000000001", "%+#25.22lld", -1LL), "# flag ignored in d");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("Hallo heimur", "%+s", "Hallo heimur"), "+ flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1024", "%+u", 1024), "+ flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("4294966272", "%+u", 4294966272U), "+ flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("x", "%+c", 'x'), "+ flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("Hallo heimur", "% s", "Hallo heimur"), "' ' flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1024", "% u", 1024), "' ' flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("4294966272", "% u", 4294966272U), "' ' flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("x", "% c", 'x'), "' ' flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("+1024", "%+ d", 1024), "' ' flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-1024", "%+ d", -1024), "' ' flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("+1024", "%+ i", 1024), "' ' flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-1024", "%+ i", -1024), "' ' flag ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("Hallo               ", "%0-20s", "Hallo"), "0 ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1024                ", "%0-20d", 1024), "0 ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-1024               ", "%0-20d", -1024), "0 ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1024                ", "%0-20i", 1024), "0 ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("-1024               ", "%0-20i", -1024), "0 ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1024                ", "%0-20u", 1024), "0 ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("4294966272          ", "%0-20u", 4294966272U), "0 ignored");
    MOS_TEST_EXPECT_WARNING(PRINTF_TEST("x                   ", "%-020c", 'x'), "0 ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("Hallo heimur", "%+ s", "Hallo heimur"), "+, ' ' flag ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("1024", "%+ u", 1024), "' ' flag ignored, + ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("4294966272", "%+ u", 4294966272U), "' ' flag ignored, + ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("x", "%+ c", 'x'), "' ' flag ignored, + ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("+01024              ", "% -0+*.*d", 20, 5, 1024), "' ' flag and 0 flag ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("-01024              ", "% -0+*.*d", 20, 5, -1024), "' ' flag and 0 flag ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("+01024              ", "% -0+*.*i", 20, 5, 1024), "' ' flag and 0 flag ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("-01024              ", "% 0-+*.*i", 20, 5, -1024), "' ' flag and 0 flag ignored");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("   0018446744073709551615", "%#+25.22llu", -1LL), "#, + ignored in u");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("   0018446744073709551615", "%#+25.22llu", -1LL), "#, + ignored in u");
    MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("+00100  ", "%#-+ 08.5lld", 100LL), "ignored 0 by -, ' ' by +, # in d");
    MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("     0000018446744073709551615", "%#+30.25llu", -1LL), "#, + ignored in u");
    MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("4294966272          ", "% 0-+*.*u", 20, 5, 4294966272U), "' ' and 0 ignored, + ignore in u");
    MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("01024               ", "% 0-+*.*u", 20, 5, 1024), "' ' and 0 ignored, + ignore in u");
    MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("Hallo               ", "% -0+*.*s", 20, 5, "Hallo heimur"), "ignore 0 by -, ' ' by +, + by s");

    MOS_TEST_CONDITIONAL(printf_tests_enable_posix)
    {
        PRINTF_TEST("Hot Pocket", "%1$s %2$s", "Hot", "Pocket");
        PRINTF_TEST("12.0 Hot Pockets", "%1$.1f %2$s %3$ss", 12.0, "Hot", "Pocket");
    }

    MOS_TEST_CONDITIONAL(printf_tests_enable_floats)
    {
        PRINTF_TEST("0.33", "%.*f", 2, 0.33333333);
        PRINTF_TEST("42.90", "%.2f", 42.8952);
        PRINTF_TEST("42.90", "%.2F", 42.8952);
        PRINTF_TEST("42.8952000000", "%.10f", 42.8952);
        PRINTF_TEST("42.90", "%1.2f", 42.8952);
        PRINTF_TEST(" 42.90", "%6.2f", 42.8952);
        PRINTF_TEST("+42.90", "%+6.2f", 42.8952);
        PRINTF_TEST("42.8952000000", "%5.10f", 42.8952);
        PRINTF_TEST("      3.14", "%*.*f", 10, 2, 3.14159265);
        PRINTF_TEST("3.14      ", "%-*.*f", 10, 2, 3.14159265);
        PRINTF_TEST("8.6000", "%2.4f", 8.6);
        PRINTF_TEST("0.600000", "%0f", 0.6);
        PRINTF_TEST("1", "%.0f", 0.6);
    }

    MOS_TEST_CONDITIONAL(printf_tests_enable_egp)
    {
        PRINTF_TEST("0x39", "%p", (void *) 57ULL);
        PRINTF_TEST("0x39", "%p", (void *) 57U);
        PRINTF_TEST("8.6000e+00", "%2.4e", 8.6);
        PRINTF_TEST(" 8.6000e+00", "% 2.4e", 8.6);
        PRINTF_TEST("-8.6000e+00", "% 2.4e", -8.6);
        PRINTF_TEST("+8.6000e+00", "%+2.4e", 8.6);
        PRINTF_TEST("8.6", "%2.4g", 8.6);
        // e
        PRINTF_TEST("+7.894561230000000e+08", "%+#22.15e", 7.89456123e8);
        PRINTF_TEST("7.894561230000000e+08 ", "%-#22.15e", 7.89456123e8);
        PRINTF_TEST(" 7.894561230000000e+08", "%#22.15e", 7.89456123e8);

        // g
        PRINTF_TEST("8.e+08", "%#1.1g", 7.89456123e8);
    }
    MOS_TEST_CONDITIONAL(printf_tests_enable_oxX)
    {
        PRINTF_TEST("0", "%#o", 0U);
        PRINTF_TEST("0", "%#x", 0U);
        PRINTF_TEST("0", "%#X", 0U);
        PRINTF_TEST("12", "%o", 10);
        PRINTF_TEST("61", "%hhx", 'a');
        PRINTF_TEST("777", "%o", 511);
        PRINTF_TEST("777", "%1o", 511);
        PRINTF_TEST("0777", "%#o", 511);
        PRINTF_TEST("2345", "%hx", 74565);
        PRINTF_TEST("00000001", "%#08o", 1);
        PRINTF_TEST("0x00000001", "%#04.8x", 1);
        PRINTF_TEST("0x0000000001", "%#012x", 1);
        PRINTF_TEST("1234abcd", "%x", 305441741);
        PRINTF_TEST("1234ABCD", "%X", 305441741);
        PRINTF_TEST("1234ABCD", "%1X", 305441741);
        PRINTF_TEST("1234abcd", "%1x", 305441741);
        PRINTF_TEST("edcb5433", "%x", 3989525555U);
        PRINTF_TEST("EDCB5433", "%X", 3989525555U);
        PRINTF_TEST("edcb5433", "%1x", 3989525555U);
        PRINTF_TEST("EDCB5433", "%1X", 3989525555U);
        PRINTF_TEST("00144   ", "%#-8.5llo", 100LL);
        PRINTF_TEST("0x1234abcd", "%#x", 305441741);
        PRINTF_TEST("0X1234ABCD", "%#X", 305441741);
        PRINTF_TEST("0xedcb5433", "%#x", 3989525555U);
        PRINTF_TEST("37777777001", "%o", 4294966785U);
        PRINTF_TEST("0XEDCB5433", "%#X", 3989525555U);
        PRINTF_TEST("37777777001", "%1o", 4294966785U);
        PRINTF_TEST("037777777001", "%#o", 4294966785U);
        PRINTF_TEST("                 777", "%*o", 20, 511);
        PRINTF_TEST("         37777777001", "%*o", 20, 4294966785U);
        PRINTF_TEST("            1234abcd", "%*x", 20, 305441741);
        PRINTF_TEST("            edcb5433", "%*x", 20, 3989525555U);
        PRINTF_TEST("            1234ABCD", "%*X", 20, 305441741);
        PRINTF_TEST("            EDCB5433", "%*X", 20, 3989525555U);
        PRINTF_TEST("                 777", "%20o", 511);
        PRINTF_TEST("            1234abcd", "%20x", 305441741);
        PRINTF_TEST("            1234ABCD", "%20X", 305441741);
        PRINTF_TEST("         37777777001", "%20o", 4294966785U);
        PRINTF_TEST("            edcb5433", "%20x", 3989525555U);
        PRINTF_TEST("            EDCB5433", "%20X", 3989525555U);
        PRINTF_TEST("777                 ", "%-20o", 511);
        PRINTF_TEST("1234abcd            ", "%-20x", 305441741);
        PRINTF_TEST("37777777001         ", "%-20o", 4294966785U);
        PRINTF_TEST("edcb5433            ", "%-20x", 3989525555U);
        PRINTF_TEST("1234ABCD            ", "%-20X", 305441741);
        PRINTF_TEST("EDCB5433            ", "%-20X", 3989525555U);
        PRINTF_TEST("00000000000000000777", "%020o", 511);
        PRINTF_TEST("00000000037777777001", "%020o", 4294966785U);
        PRINTF_TEST("0000000000001234abcd", "%020x", 305441741);
        PRINTF_TEST("000000000000edcb5433", "%020x", 3989525555U);
        PRINTF_TEST("0000000000001234ABCD", "%020X", 305441741);
        PRINTF_TEST("000000000000EDCB5433", "%020X", 3989525555U);
        PRINTF_TEST("                0777", "%#20o", 511);
        PRINTF_TEST("        037777777001", "%#20o", 4294966785U);
        PRINTF_TEST("          0x1234abcd", "%#20x", 305441741);
        PRINTF_TEST("          0xedcb5433", "%#20x", 3989525555U);
        PRINTF_TEST("          0X1234ABCD", "%#20X", 305441741);
        PRINTF_TEST("          0XEDCB5433", "%#20X", 3989525555U);
        PRINTF_TEST("00000000000000000777", "%#020o", 511);
        PRINTF_TEST("00000000037777777001", "%#020o", 4294966785U);
        PRINTF_TEST("0x00000000001234abcd", "%#020x", 305441741);
        PRINTF_TEST("0x0000000000edcb5433", "%#020x", 3989525555U);
        PRINTF_TEST("0X00000000001234ABCD", "%#020X", 305441741);
        PRINTF_TEST("0X0000000000EDCB5433", "%#020X", 3989525555U);
        PRINTF_TEST("00000000000000000777", "%.20o", 511);
        PRINTF_TEST("00000000037777777001", "%.20o", 4294966785U);
        PRINTF_TEST("0000000000001234abcd", "%.20x", 305441741);
        PRINTF_TEST("000000000000edcb5433", "%.20x", 3989525555U);
        PRINTF_TEST("0000000000001234ABCD", "%.20X", 305441741);
        PRINTF_TEST("000000000000EDCB5433", "%.20X", 3989525555U);
        PRINTF_TEST("               00777", "%20.5o", 511);
        PRINTF_TEST("         37777777001", "%20.5o", 4294966785U);
        PRINTF_TEST("            1234abcd", "%20.5x", 305441741);
        PRINTF_TEST("          00edcb5433", "%20.10x", 3989525555U);
        PRINTF_TEST("            1234ABCD", "%20.5X", 305441741);
        PRINTF_TEST("          00EDCB5433", "%20.10X", 3989525555U);
        PRINTF_TEST("               00777", "%020.5o", 511);
        PRINTF_TEST("         37777777001", "%020.5o", 4294966785U);
        PRINTF_TEST("            1234abcd", "%020.5x", 305441741);
        PRINTF_TEST("          00edcb5433", "%020.10x", 3989525555U);
        PRINTF_TEST("            1234ABCD", "%020.5X", 305441741);
        PRINTF_TEST("          00EDCB5433", "%020.10X", 3989525555U);
        PRINTF_TEST("                 777", "%20.o", 511);
        PRINTF_TEST("                    ", "%20.o", 0U);
        PRINTF_TEST("            1234abcd", "%20.x", 305441741);
        PRINTF_TEST("                    ", "%20.x", 0U);
        PRINTF_TEST("            1234ABCD", "%20.X", 305441741);
        PRINTF_TEST("                    ", "%20.X", 0U);
        PRINTF_TEST("         37777777001", "%20.0o", 4294966785U);
        PRINTF_TEST("            edcb5433", "%20.0x", 3989525555U);
        PRINTF_TEST("            EDCB5433", "%20.0X", 3989525555U);
        PRINTF_TEST("0001777777777777777777634", "%#.25llo", -100LL);
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("0x01    ", "%#-08.2x", 1), "0 flag ignored by '-'");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST(" 01777777777777777777634", "%#+24.20llo", -100LL), "+ ignored in o");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("001777777777777777777634", "%#+20.24llo", -100LL), "+ ignored in o");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST(" 0x00ffffffffffffff9c", "%#+21.18llx", -100LL), "+ ignored in x");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("0X00000FFFFFFFFFFFFFF9C", "%#+18.21llX", -100LL), "+ ignored in X");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("777                 ", "%-020o", 511), "- flag ignored in o mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("37777777001         ", "%-020o", 4294966785U), "- flag ignored in o mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1234abcd            ", "%-020x", 305441741), "- flag ignored in x mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("edcb5433            ", "%-020x", 3989525555U), "- flag ignored in x mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1234ABCD            ", "%-020X", 305441741), "- flag ignored in X mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("EDCB5433            ", "%-020X", 3989525555U), "- flag ignored in X mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("777", "%+o", 511), "+ flag ignored in o mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("37777777001", "%+o", 4294966785U), "+ flag ignored in o mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1234abcd", "%+x", 305441741), "+ flag ignored in x mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("edcb5433", "%+x", 3989525555U), "+ flag ignored in x mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1234ABCD", "%+X", 305441741), "+ flag ignored in X mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("EDCB5433", "%+X", 3989525555U), "+ flag ignored in X mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("777", "% o", 511), "' ' flag ignored in o mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("37777777001", "% o", 4294966785U), "' ' flag ignored in o mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1234abcd", "% x", 305441741), "' ' flag ignored in x mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("edcb5433", "% x", 3989525555U), "' ' flag ignored in x mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("1234ABCD", "% X", 305441741), "' ' flag ignored in x mode");
        MOS_TEST_EXPECT_WARNING(PRINTF_TEST("EDCB5433", "% X", 3989525555U), "' ' flag ignored in x mode");
        MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("777", "%+ o", 511), "+ and ' ' ignored in o mode");
        MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("37777777001", "%+ o", 4294966785U), "+ and ' ' ignored in o mode");
        MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("1234abcd", "%+ x", 305441741), "+ and ' ' ignored in x mode");
        MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("edcb5433", "%+ x", 3989525555U), "+ and ' ' ignored in x mode");
        MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("1234ABCD", "%+ X", 305441741), "+ and ' ' ignored in X mode");
        MOS_TEST_EXPECT_WARNING_N(2, PRINTF_TEST("EDCB5433", "%+ X", 3989525555U), "+ and ' ' ignored in X mode");
        MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("00777               ", "%+ -0*.*o", 20, 5, 511), "ignored ' ' and 0, + in o");
        MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("37777777001         ", "%+ -0*.*o", 20, 5, 4294966785U), "ignored ' ' and 0, + in o");
        MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("1234abcd            ", "%+ -0*.*x", 20, 5, 305441741), "ignored ' ' and 0, + in x");
        MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("00edcb5433          ", "%+ -0*.*x", 20, 10, 3989525555U), "ignored ' ' and 0, + in x");
        MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("1234ABCD            ", "% -+0*.*X", 20, 5, 305441741), "ignored ' ' and 0, + in X");
        MOS_TEST_EXPECT_WARNING_N(3, PRINTF_TEST("00EDCB5433          ", "% -+0*.*X", 20, 10, 3989525555U), "ignored ' ' and 0, + in X");
    }
}
MOS_WARNING_POP
