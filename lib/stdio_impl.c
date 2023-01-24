// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/mos_lib.h"
#include "lib/stdio.h"
#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/mos_global.h"

typedef enum
{
    LM_none,
    // char
    LM_hh,
    // short
    LM__h,
    // long
    LM__l,
    // long long
    LM_ll,
    // long double
    LM__L,
    // intmax_t
    LM__j,
    // size_t
    LM__z,
    // ptrdiff_t
    LM__t,
} length_modifier_t;

typedef struct
{
    bool left_aligned : 1;      // -
    bool show_sign : 1;         // +
    bool space_if_positive : 1; // prepend space if positive
    bool pad_with_zero : 1;     // padding with zero

    // For g and G types, trailing zeros are not removed.
    // For f, F, e, E, g, G types, the output always contains a decimal point.
    // For o, x, X types, the text 0, 0x, 0X, respectively, is prepended to non-zero numbers.
    bool hash : 1;

    // POSIX extension '\'' not implemented
    // glibc 2.2 extension 'I' not implemented

    // If the converted value has fewer characters than the field width, it will be padded with spaces on the left
    // (or right, if the left-adjustment flag has been given).
    // A negative field width is taken as a '-' flag followed by a positive field width.
    // In no case does a nonexistent or small field width cause truncation of a field;
    // if the result of a conversion is wider than the field width, the field is expanded to contain the conversion result.
    s32 minimum_width : 24;

    bool has_explicit_precision : 1;
    // `d, i, o, u, x, X` -> The minimum number of digits to appear;
    // `a, A, e, E, f, F` -> The number of digits to appear after the radix character;
    // `g, G`             -> The maximum number of significant digits;
    // `s, S`             -> The maximum number of characters to be printed from a string;
    s32 precision : 16;

    length_modifier_t length : 8;
} __packed printf_flags_t;

typedef enum
{
    BASE_8 = 8,
    BASE_10 = 10,
    BASE_16 = 16,
} base_t;

typedef struct
{
    va_list real;
} va_list_ptrwrappper_t;

static size_t parse_printf_flags(const char *format, printf_flags_t *pflags, va_list_ptrwrappper_t *args)
{
    const char *start = format;
#define goto_next_char() (format++)
#define current          (*format)

    while (1)
    {
        switch (current)
        {
            case '-': pflags->left_aligned = true; break;
            case '+': pflags->show_sign = true; break;
            case ' ': pflags->space_if_positive = true; break;
            case '#': pflags->hash = true; break;
            // We can read multiple 0s and leave the 'width' field empty, 0-width is meaningless.
            case '0': pflags->pad_with_zero = true; break;
            default: goto flag_parse_done;
        }
        goto_next_char();
    };

flag_parse_done:
    // width
    pflags->minimum_width = 0;
    if (current == '*')
    {
        pflags->minimum_width = va_arg(args->real, s32), goto_next_char();
        if (pflags->minimum_width < 0)
        {
            // A negative field width is taken as a '-' flag followed by a positive field width
            pflags->left_aligned = true;
            pflags->minimum_width = -pflags->minimum_width;
        }
    }
    else
    {
        while ('0' <= current && current <= '9')
            pflags->minimum_width = (pflags->minimum_width * 10 + (current - '0')), goto_next_char();
    }

    // precision
    pflags->precision = 0;
    if (current == '.')
    {
        pflags->has_explicit_precision = true;
        goto_next_char();
        if (current == '*')
        {
            pflags->precision = va_arg(args->real, s32), goto_next_char();
            // A negative precision argument is taken as if the precision were omitted.
            if (pflags->precision < 0)
                pflags->precision = 0, pflags->has_explicit_precision = false;
        }
        else
        {
            while ('0' <= current && current <= '9')
                pflags->precision = pflags->precision * 10 + current - '0', goto_next_char();
        }
    }

    // length enums
    if (current == 'h')
    {
        goto_next_char();
        if (current == 'h')
            pflags->length = LM_hh, goto_next_char();
        else
            pflags->length = LM__h;
    }
    else if (current == 'l')
    {
        goto_next_char();
        if (current == 'l')
            pflags->length = LM_ll, goto_next_char();
        else
            pflags->length = LM__l;
    }
    else if (current == 'L')
    {
        goto_next_char();
        pflags->length = LM__L;
    }
    else if (current == 'j')
    {
        goto_next_char();
        pflags->length = LM__j;
    }
    else if (current == 'z')
    {
        goto_next_char();
        pflags->length = LM__z;
    }
    else if (current == 't')
    {
        goto_next_char();
        pflags->length = LM__t;
    }

#undef goto_next_char
#undef current

    if (unlikely(pflags->left_aligned && pflags->pad_with_zero))
    {
        pflags->pad_with_zero = false;
        mos_warn("printf: '0' flag is ignored by the '-' flag");
    }

    if (unlikely(pflags->show_sign && pflags->space_if_positive))
    {
        pflags->space_if_positive = false;
        mos_warn("printf: ' ' flag is ignored by the '+' flag");
    }

    return format - start;
}

// writes a character into the buffer, and increses the buffer pointer
should_inline void buf_putchar(char **pbuf, char c)
{
    **pbuf = c;
    (*pbuf)++;
}

static const char *const lower_hex_digits = "0123456789abcdef";
static const char *const upper_hex_digits = "0123456789ABCDEF";

// ! prints d, i, o, u, x, and X
static int printf_diouxX(char *buf, u64 number, printf_flags_t *pflags, char conv)
{
    MOS_LIB_ASSERT(conv == 'd' || conv == 'i' || conv == 'o' || conv == 'u' || conv == 'x' || conv == 'X');
    MOS_LIB_ASSERT(pflags->precision >= 0);
    MOS_LIB_ASSERT(pflags->minimum_width >= 0);

    if (conv == 'd' || conv == 'i' || conv == 'u')
    {
        if (unlikely(pflags->hash))
        {
            mos_warn("'#' flag is ignored in d, i mode");
            pflags->hash = false;
        }
    }

    base_t base = BASE_10;
    bool upper_case = false;
    const char *hex_digits = NULL;

    bool is_unsigned_ouxX = conv == 'o' || conv == 'u' || conv == 'x' || conv == 'X';
    if (is_unsigned_ouxX)
    {
        if (unlikely(pflags->show_sign))
        {
            mos_warn("'+' flag is ignored in unsigned mode");
            pflags->show_sign = false;
        }
        if (unlikely(pflags->space_if_positive))
        {
            mos_warn("' ' flag is ignored in unsigned mode");
            pflags->space_if_positive = false;
        }

        base = (conv == 'o' ? BASE_8 : (conv == 'u' ? BASE_10 : BASE_16));
        upper_case = conv == 'X';
        hex_digits = upper_case ? upper_hex_digits : lower_hex_digits;

        switch (pflags->length)
        {
            case LM_hh: number = (u8) number; break;
            case LM__h: number = (u16) number; break;
            case LM_none:
            case LM__l: number = (u32) number; break;
            case LM_ll: number = (u64) number; break;
            case LM__z: number = (size_t) number; break;
            case LM__t: number = (ptrdiff_t) number; break;
            case LM__L: number = (u64) number; break;
            case LM__j:
            default: MOS_LIB_UNREACHABLE();
        }
    }

    // If a precision is given with a numeric conversion (d, i, o, u, x, and X), the 0 flag is ignored.
    if (pflags->has_explicit_precision && pflags->pad_with_zero)
        pflags->pad_with_zero = false;

    // The default precision is 1 for d, i, o, u, x, and X
    if (!pflags->has_explicit_precision)
        pflags->precision = 1;

    char *start = buf;

    {
        char num_prefix_buf[5] = { 0 };
        char num_content_buf[32] = { 0 };

        // Setup prefixes.
        if (base == BASE_10 && !is_unsigned_ouxX)
        {
            bool is_negative = ((s64) number) < 0;
            if (is_negative)
                number = -(s64) number, num_prefix_buf[0] = '-';
            else if (pflags->show_sign)
                num_prefix_buf[0] = '+'; // 0 is positive too !!!!!
            else if (pflags->space_if_positive)
                num_prefix_buf[0] = ' ';
        }
        else if (base == BASE_16 && pflags->hash)
        {
            // '#' flag turns on prefix '0x' or '0X' for hexadecimal conversions.
            // For x and X conversions, a nonzero result has the string "0x" (or "0X" for X conversions) prepended to it.
            if (number != 0)
            {
                num_prefix_buf[0] = '0';
                num_prefix_buf[1] = upper_case ? 'X' : 'x';
            }
        }
        else if (base == BASE_8 && pflags->hash)
        {
            // ! BEGIN SPECIAL CASE for base 8 + '#' flag
            // to be handled later, when we are printing the number
            // ! END SPECIAL CASE
        }

        // Print the number.
        if (number == 0)
        {
            // When 0 is printed with an explicit precision 0, the output is empty.
            // so only print a '0' if the precision is **not** 0.
            if (pflags->precision != 0)
                num_content_buf[0] = '0';
        }
        else
        {
            char *pnumberbuf = num_content_buf;
            switch (base)
            {
                case BASE_8:
                case BASE_10:
                    while (number > 0)
                        buf_putchar(&pnumberbuf, '0' + (char) (number % base)), number /= base;
                    break;
                case BASE_16:
                    while (number > 0)
                        buf_putchar(&pnumberbuf, hex_digits[number % 16]), number /= 16;
                    break;
                default: MOS_LIB_UNREACHABLE();
            }
        }

        s32 n_digits = strlen(num_content_buf);

        // ! BEGIN SPECIAL CASE for base 8 + '#' flag
        if (base == BASE_8 && pflags->hash)
        {
            // if there's no padding needed, we'll have to add the '0' prefix
            if (pflags->precision - n_digits <= 0 && num_content_buf[0] != '0')
                num_prefix_buf[0] = '0';
        }
        // ! END SPECIAL CASE

        s32 precision_padding = MAX(pflags->precision - n_digits, 0);
        s32 width_to_pad = MAX(pflags->minimum_width - strlen(num_prefix_buf) - precision_padding - n_digits, 0);

        char *pnum_prefix = num_prefix_buf;
        char *pnum_content = num_content_buf + n_digits;
        if (pflags->left_aligned)
        {
            while (*pnum_prefix)
                buf_putchar(&buf, *pnum_prefix++);
            while (precision_padding-- > 0)
                buf_putchar(&buf, '0');
            while (pnum_content > num_content_buf)
                buf_putchar(&buf, *--pnum_content);
            while (width_to_pad-- > 0)
                buf_putchar(&buf, ' ');
        }
        else
        {
            if (pflags->pad_with_zero)
            {
                // zero should be after the sign
                while (*pnum_prefix)
                    buf_putchar(&buf, *pnum_prefix++);
                while (width_to_pad-- > 0)
                    buf_putchar(&buf, '0');
            }
            else
            {
                // space should be before the sign
                while (width_to_pad-- > 0)
                    buf_putchar(&buf, ' ');
                while (*pnum_prefix)
                    buf_putchar(&buf, *pnum_prefix++);
            }
            while (precision_padding-- > 0)
                buf_putchar(&buf, '0');
            while (pnum_content > num_content_buf)
                buf_putchar(&buf, *--pnum_content);
        }
    }
    return buf - start;
}

static int printf_cs(char *buf, char *data, printf_flags_t *pflags, char conv)
{
    MOS_LIB_ASSERT(conv == 'c' || conv == 's');
    MOS_LIB_ASSERT(pflags->precision >= 0);
    MOS_LIB_ASSERT(pflags->minimum_width >= 0);

    if (unlikely(pflags->hash))
    {
        mos_warn("printf: '#' flag is ignored in 'c' and 's' mode.");
        pflags->hash = false;
    }

    if (unlikely(pflags->pad_with_zero))
    {
        mos_warn("printf: '0' flag is ignored in 'c' and 's' mode.");
        pflags->pad_with_zero = false;
    }

    if (unlikely(pflags->show_sign))
    {
        mos_warn("printf: '+' flag is ignored in 'c' and 's' mode.");
        pflags->show_sign = false;
    }

    if (unlikely(pflags->space_if_positive))
    {
        mos_warn("printf: ' ' flag is ignored in 'c' and 's' mode.");
        pflags->space_if_positive = false;
    }

    if (unlikely(conv == 'c' && pflags->has_explicit_precision))
    {
        mos_warn("printf: precision is ignored in 'c' mode.");
        pflags->has_explicit_precision = false;
        pflags->precision = 0;
    }

    char *start = buf;

    s32 printed_len = conv == 'c' ? 1 : strlen(data);

    // If presition is given, the string is truncated to this length.
    if (pflags->has_explicit_precision)
        printed_len = MIN(printed_len, pflags->precision);

    s32 width_to_pad = MAX(pflags->minimum_width - printed_len, 0);

    if (pflags->left_aligned)
    {
        while (printed_len-- > 0)
            buf_putchar(&buf, *data++);
        while (width_to_pad-- > 0)
            buf_putchar(&buf, ' ');
    }
    else
    {
        while (width_to_pad-- > 0)
            buf_putchar(&buf, ' ');
        while (printed_len-- > 0)
            buf_putchar(&buf, *data++);
    }

    return buf - start;
}

int vsnprintf(char *buf, size_t size, const char *format, va_list _args)
{
    va_list_ptrwrappper_t args;
    va_copy(args.real, _args);
    va_end(_args);

#pragma message "TODO: Check for buffer overflow."
    MOS_UNUSED(size);
    char *start = buf;

    for (; *format; format++)
    {
        if (*format == '%')
        {
            // skip '%'
            format++;

            // parse flags
            printf_flags_t flags = { 0 };
            size_t c = parse_printf_flags(format, &flags, &args);
            format += c;

            switch (*format)
            {
                case 'd':
                case 'i':
                case 'o':
                case 'u':
                case 'x':
                case 'X':
                {
                    // print a signed integer
                    u64 value = 0;
                    switch (flags.length)
                    {
                        case LM_hh: value = (s8) va_arg(args.real, s32); break;
                        case LM__h: value = (s16) va_arg(args.real, s32); break;
                        case LM_none:
                        case LM__l: value = va_arg(args.real, s32); break;
                        case LM_ll: value = va_arg(args.real, s64); break;
                        case LM__z: value = va_arg(args.real, size_t); break;
                        case LM__t: value = va_arg(args.real, ptrdiff_t); break;
                        case LM__L: value = va_arg(args.real, s64); break;
                        case LM__j:
                        default: MOS_LIB_UNREACHABLE();
                    }
                    int c = printf_diouxX(buf, value, &flags, *format);
                    buf += c;
                    break;
                }

                // default precision ; for e, E, f, g, and G it is 0.
                case 'f':
                case 'F':
                {
                    // print a floating point number
                    MOS_LIB_UNIMPLEMENTED("printf: %f / %F");
                    break;
                }
                case 'e':
                case 'E':
                {
                    // print a floating point number in scientific notation
                    MOS_LIB_UNIMPLEMENTED("printf: %e / %E");
                    break;
                }
                case 'g':
                case 'G':
                {
                    // print a floating point number in scientific notation
                    MOS_LIB_UNIMPLEMENTED("printf: %g / %G");
                    break;
                }
                case 's':
                {
                    // print string
                    char *string = va_arg(args.real, char *);
                    int c = printf_cs(buf, string, &flags, *format);
                    buf += c;
                    break;
                }
                case 'c':
                {
                    // print a character
                    char value = (char) va_arg(args.real, s32);
                    int c = printf_cs(buf, &value, &flags, *format);
                    buf += c;
                    break;
                }
                case 'p':
                {
                    // print a pointer
                    u64 value = (size_t) va_arg(args.real, void *);
                    buf_putchar(&buf, '0');
                    buf_putchar(&buf, 'x');
                    int c = printf_diouxX(buf, value, &flags, 'x');
                    buf += c;
                    break;
                }
                case 'a':
                case 'A':
                {
                    // print a hexadecimal number in ASCII
                    MOS_LIB_UNIMPLEMENTED("printf: %a / %A");
                    break;
                }
                case 'n':
                {
                    // print the number of characters printed
                    MOS_LIB_UNIMPLEMENTED("printf: %n");
                    break;
                }
                case '%':
                {
                    // print a '%'
                    buf_putchar(&buf, '%');
                    break;
                }
                case '\0':
                {
                    // end of format string
                    mos_warn("printf: incomplete format specifier");
                    goto end;
                }
                default:
                {
                    mos_warn("printf: unknown format specifier");
                    buf_putchar(&buf, '%');
                    buf_putchar(&buf, *format);
                    break;
                }
            }
        }
        else
        {
            buf_putchar(&buf, *format);
        }
    }
end:
    buf_putchar(&buf, 0);

    va_end(args.real);
    return buf - start;
}
