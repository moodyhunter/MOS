// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/stdio.h"

#include "mos/attributes.h"
#include "mos/bug.h"
#include "mos/kernel.h"
#include "mos/stdlib.h"
#include "mos/string.h"

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

    bool has_precision : 1;
    // `d, i, o, u, x, X` -> The minimum number of digits to appear;
    // `a, A, e, E, f, F` -> The number of digits to appear after the radix character;
    // `g, G`             -> The maximum number of significant digits;
    // `s, S`             -> The maximum number of characters to be printed from a string;
    s32 precision : 16;

    length_modifier_t length : 8;
} __attr_packed printf_flags_t;

static size_t parse_printf_flags(const char *format, printf_flags_t *pflags, va_list *args)
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
        pflags->minimum_width = va_arg(*args, s32), goto_next_char();
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
        pflags->has_precision = true;
        goto_next_char();
        if (current == '*')
        {
            pflags->precision = va_arg(*args, s32), goto_next_char();
            // A negative precision argument is taken as if the precision were omitted.
            if (pflags->precision < 0)
                pflags->precision = 0, pflags->has_precision = false;
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
        warning("printf: '0' flag is ignored in '-' mode");
    }

    if (unlikely(pflags->show_sign && pflags->space_if_positive))
    {
        pflags->space_if_positive = false;
        warning("printf: ' ' flag is ignored in '+' mode");
    }

    return format - start;
}

// writes a character into the buffer, and increses the buffer pointer
void _putchar(char **pbuf, char c)
{
    **pbuf = c;
    (*pbuf)++;
}

// ! prints d, i, o, u, x, and X
static int printf_diouxX(char *pbuf, u64 number, printf_flags_t *pflags, char conv)
{
    MOS_ASSERT(conv == 'd' || conv == 'i' || conv == 'o' || conv == 'u' || conv == 'x' || conv == 'X');
    MOS_ASSERT(pflags->precision >= 0);
    MOS_ASSERT(pflags->minimum_width >= 0);

    if (unlikely(pflags->hash))
        warning("'#' flag is ignored in diouxX");

    if (conv == 'u')
    {
        if (unlikely(pflags->show_sign))
        {
            warning("'+' flag is ignored in u mode");
            pflags->show_sign = false;
        }

        if (unlikely(pflags->space_if_positive))
        {
            warning("' ' flag is ignored in u mode");
            pflags->space_if_positive = false;
        }
    }

    // If a precision is given with a numeric conversion (d, i, o, u, x, and X), the 0 flag is ignored.
    if (pflags->has_precision && pflags->pad_with_zero)
        pflags->pad_with_zero = false;

    if (conv == 'u')
    {
        switch (pflags->length)
        {
            case LM_hh: number = (u8) number; break;
            case LM__h: number = (u16) number; break;
            case LM__l: number = (u32) number; break;
            case LM_ll: number = (u64) number; break;
            case LM__j: number = (uintmax_t) number; break;
            case LM__z: number = (size_t) number; break;
            case LM__t: number = (ptrdiff_t) number; break;
            case LM__L: number = (u64) number; break;
            case LM_none: number = (u32) number; break;
            default: MOS_UNREACHABLE();
        }
    }

    char *start = pbuf;
    {
        char numberbuf[32] = { 0 };
        char *pnumberbuf = numberbuf;

        s32 width_to_pad = pflags->minimum_width;
        s32 precision = pflags->precision;

        char sign = '\0';

        bool is_negative = conv != 'u' && ((s64) number) < 0;
        if (is_negative)
            number = -(s64) number, sign = '-';
        else if (pflags->show_sign)
            sign = '+'; // 0 is positive too !!!!!
        else if (pflags->space_if_positive)
            sign = ' ';

        if (sign)
            width_to_pad--;

        s32 digits = 0;
        if (number == 0)
        {
            if (pflags->has_precision && precision == 0)
                ; // do nothing, "A precision of 0 means that no character is written for the value 0."
            else
                _putchar(&pnumberbuf, '0'), digits++;
        }
        else
        {
            while (number > 0)
            {
                _putchar(&pnumberbuf, (char) (number % 10) + '0');
                number /= 10, digits++;
            }
        }

        precision = MAX(precision - digits, 0);
        width_to_pad = MAX(width_to_pad - precision - digits, 0);

        if (pflags->left_aligned)
        {
            if (sign)
                _putchar(&pbuf, sign);
            while (precision-- > 0)
                _putchar(&pbuf, '0');
            while (pnumberbuf > numberbuf)
                _putchar(&pbuf, *--pnumberbuf);
            while (width_to_pad-- > 0)
                _putchar(&pbuf, ' ');
        }
        else
        {
            if (pflags->pad_with_zero)
            {
                // zero should be after the sign
                if (sign)
                    _putchar(&pbuf, sign);
                while (width_to_pad-- > 0)
                    _putchar(&pbuf, '0');
            }
            else
            {
                // space should be before the sign
                while (width_to_pad-- > 0)
                    _putchar(&pbuf, ' ');
                if (sign)
                    _putchar(&pbuf, sign);
            }
            while (precision-- > 0)
                _putchar(&pbuf, '0');
            while (pnumberbuf > numberbuf)
                _putchar(&pbuf, *--pnumberbuf);
        }
    }
    return pbuf - start;
}

static int printf_cs(char *pbuf, char *data, printf_flags_t *pflags, char conv)
{
    MOS_ASSERT(conv == 'c' || conv == 's');
    MOS_ASSERT(pflags->precision >= 0);
    MOS_ASSERT(pflags->minimum_width >= 0);

    if (unlikely(pflags->hash))
    {
        warning("printf: '#' flag is ignored in 'c' and 's' mode.");
        pflags->hash = false;
    }

    if (unlikely(pflags->pad_with_zero))
    {
        warning("printf: '0' flag is ignored in 'c' and 's' mode.");
        pflags->pad_with_zero = false;
    }

    if (unlikely(pflags->show_sign))
    {
        warning("printf: '+' flag is ignored in 'c' and 's' mode.");
        pflags->show_sign = false;
    }

    if (unlikely(pflags->space_if_positive))
    {
        warning("printf: ' ' flag is ignored in 'c' and 's' mode.");
        pflags->space_if_positive = false;
    }

    if (unlikely(conv == 'c' && pflags->has_precision))
    {
        warning("printf: precision is ignored in 'c' mode.");
        pflags->has_precision = false;
        pflags->precision = 0;
    }

    char *start = pbuf;

    s32 printed_len = conv == 'c' ? 1 : strlen(data);

    // If presition is given, the string is truncated to this length.
    if (pflags->has_precision)
        printed_len = MIN(printed_len, pflags->precision);

    s32 width_to_pad = MAX(pflags->minimum_width - printed_len, 0);

    if (pflags->left_aligned)
    {
        while (printed_len-- > 0)
            _putchar(&pbuf, *data++);
        while (width_to_pad-- > 0)
            _putchar(&pbuf, ' ');
    }
    else
    {
        while (width_to_pad-- > 0)
            _putchar(&pbuf, ' ');
        while (printed_len-- > 0)
            _putchar(&pbuf, *data++);
    }

    return pbuf - start;
}

int vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
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
                    u64 value;
                    if (flags.length == LM_hh)
                        value = (schar) va_arg(args, s32); // must use promoted type
                    else if (flags.length == LM__h)
                        value = (s16) va_arg(args, s32); // must use promoted type
                    else if (flags.length == LM__l)
                        value = va_arg(args, s32);
                    else if (flags.length == LM_ll)
                        value = va_arg(args, s64);
                    else if (flags.length == LM__L)
                        value = va_arg(args, s64);
                    else if (flags.length == LM__j)
                        value = va_arg(args, s64);
                    else if (flags.length == LM__z)
                        value = va_arg(args, s64);
                    else if (flags.length == LM__t)
                        value = va_arg(args, s64);
                    else
                        value = va_arg(args, s32);

                    int c = printf_diouxX(buf, value, &flags, *format);
                    buf += c;
                    break;
                }
                case 'f':
                case 'F':
                {
                    // print a floating point number
                    break;
                }
                case 'E':
                case 'e':
                {
                    // print a floating point number in scientific notation
                    break;
                }
                case 'g':
                case 'G':
                {
                    // print a floating point number in scientific notation
                    break;
                }
                case 's':
                {
                    // print string
                    char *string = va_arg(args, char *);
                    int c = printf_cs(buf, string, &flags, *format);
                    buf += c;
                    break;
                }
                case 'c':
                {
                    // print a character
                    char value = (char) va_arg(args, s32);
                    int c = printf_cs(buf, &value, &flags, *format);
                    buf += c;
                    break;
                }
                case 'p':
                {
                    // print a pointer
                    MOS_UNIMPLEMENTED("printf: %n");
                    break;
                }
                case 'a':
                case 'A':
                {
                    // print a hexadecimal number in ASCII
                    MOS_UNIMPLEMENTED("printf: %n");
                    break;
                }
                case 'n':
                {
                    // print the number of characters printed
                    MOS_UNIMPLEMENTED("printf: %n");
                    break;
                }
                case '%':
                {
                    // print a '%'
                    _putchar(&buf, '%');
                    break;
                }
                case '\0':
                {
                    // end of format string
                    warning("printf: incomplete format specifier");
                    goto end;
                }
                default:
                {
                    // C, S, m not implemented
                    warning("printf: unknown format specifier");
                    _putchar(&buf, '%');
                    _putchar(&buf, *format);
                    break;
                }
            }
        }
        else
        {
            _putchar(&buf, *format);
        }
    }
end:
    _putchar(&buf, 0);

    va_end(args);
    return buf - start;
}
