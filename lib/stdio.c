// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdio.h"

#include "attributes.h"
#include "bug.h"
#include "stdlib.h"
#include "string.h"

#include <stdlib.h>

typedef enum
{
    LM_none,
    // char
    LM_hh,
    // short
    LM_h,
    // long
    LM_l,
    // long long
    LM_ll,
    // long double
    LM_L,
    // intmax_t
    LM_j,
    // size_t
    LM_z,
    // ptrdiff_t
    LM_t,
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

#define WIDTH_PRECISION_READ_VAARG -1

static size_t parse_printf_flags(const char *format, printf_flags_t *pflags)
{
    const char *start = format;
#define goto_next_char() (format++)
#define next_char_n(n)   (*(format + n))
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
    if (unlikely(pflags->left_aligned && pflags->pad_with_zero))
    {
        pflags->pad_with_zero = false;
        warning("printf: '0' flag is ignored in left-aligned mode");
    }

    // width
    pflags->minimum_width = 0;
    if (current == '*')
        pflags->minimum_width = WIDTH_PRECISION_READ_VAARG, goto_next_char();
    else
        while ('0' <= current && current <= '9')
            pflags->minimum_width = (pflags->minimum_width * 10 + (current - '0')), goto_next_char();

    // precision
    pflags->precision = 0;
    if (current == '.')
    {
        pflags->has_precision = true;
        goto_next_char();
        if (current == '*')
            pflags->precision = WIDTH_PRECISION_READ_VAARG, goto_next_char();
        else
            while ('0' <= current && current <= '9')
                pflags->precision = pflags->precision * 10 + current - '0', goto_next_char();
    }

    // length enums
    if (current == 'h')
    {
        goto_next_char();
        if (next_char_n(1) == 'h')
            pflags->length = LM_hh, goto_next_char();
        else
            pflags->length = LM_h;
    }
    else if (current == 'l')
    {
        goto_next_char();
        if (next_char_n(1) == 'l')
            pflags->length = LM_ll, goto_next_char();
        else
            pflags->length = LM_l;
    }
    else if (current == 'L')
    {
        goto_next_char();
        pflags->length = LM_L;
    }
    else if (current == 'j')
    {
        goto_next_char();
        pflags->length = LM_j;
    }
    else if (current == 'z')
    {
        goto_next_char();
        pflags->length = LM_z;
    }
    else if (current == 't')
    {
        goto_next_char();
        pflags->length = LM_t;
    }

#undef goto_next_char
#undef next_char_n
#undef current
    return format - start;
}

// writes a character into the buffer, and increses the buffer pointer
void putchar(char **pbuf, char c)
{
    **pbuf = c;
    (*pbuf)++;
}

void putstring(char **pbuf, const char *str)
{
    s32 len = strlen(str);
    memcpy(*pbuf, str, len);
    *pbuf += len;
}

// ! prints d, i, o, u, x, and X
int _print_number_diouxX(char *pbuf, u64 number, printf_flags_t *pflags)
{
    MOS_ASSERT(!(pflags->left_aligned && pflags->pad_with_zero));
    MOS_ASSERT(pflags->precision >= 0);
    MOS_ASSERT(pflags->minimum_width >= 0);

    if (unlikely(pflags->hash))
        warning("printf: '#' flag is ignored.");

    // If a precision is given with a numeric conversion (d, i, o, u, x, and X), the 0 flag is ignored.
    if (pflags->has_precision && pflags->pad_with_zero)
        pflags->pad_with_zero = false;

    char *start = pbuf;
    {
        char numberbuf[32] = { 0 };
        char *pnumberbuf = numberbuf;

        s32 width_to_pad = pflags->minimum_width;
        s32 precision = pflags->precision;

        char sign = '\0';

        bool is_negative = ((s64) number) < 0;
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
            putchar(&pnumberbuf, '0'), digits++;
        else
        {
            while (number > 0)
            {
                putchar(&pnumberbuf, (char) (number % 10) + '0');
                number /= 10, digits++;
            }
        }

#define MAX(a, b) ((a) > (b) ? (a) : (b))
        precision = MAX(precision - digits, 0);
        width_to_pad = MAX(width_to_pad - precision, 0);

        if (pflags->left_aligned)
        {
            warning("printf: left-aligned mode is not implemented");
        }
        else
        {
            if (pflags->pad_with_zero)
            {
                if (sign)
                    putchar(&pbuf, sign);
                while (width_to_pad-- > 0)
                    putchar(&pbuf, '0');
            }
            else
            {
                while (width_to_pad-- > 0)
                    putchar(&pbuf, ' ');
                if (sign)
                    putchar(&pbuf, sign);
            }
            while (precision-- > 0)
                putchar(&pbuf, '0');
            while (pnumberbuf > numberbuf)
                putchar(&pbuf, *--pnumberbuf);
        }
    }
    return pbuf - start;
}

int vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
    MOS_UNUSED(size);
    char **pbuf = &buf;

    char *start = buf;

    for (; *format; format++)
    {
        if (*format == '%')
        {
            // skip '%'
            format++;

            // parse flags
            printf_flags_t flags = { 0 };
            size_t c = parse_printf_flags(format, &flags);
            format += c;

            if (flags.minimum_width == WIDTH_PRECISION_READ_VAARG)
                flags.minimum_width = va_arg(args, s32);

            if (flags.precision == WIDTH_PRECISION_READ_VAARG)
                flags.precision = va_arg(args, s32);

            switch (*format)
            {
                case 'd':
                case 'i':
                {
                    // print a signed integer
                    s32 value;
                    if (flags.length == LM_hh)
                        value = (schar) va_arg(args, s32); // must use promoted type
                    else if (flags.length == LM_h)
                        value = (s16) va_arg(args, s32); // must use promoted type
                    else if (flags.length == LM_l)
                        value = va_arg(args, s32);
                    else if (flags.length == LM_ll)
                        value = va_arg(args, s64);
                    else if (flags.length == LM_L)
                        value = va_arg(args, s64);
                    else if (flags.length == LM_j)
                        value = va_arg(args, s64);
                    else if (flags.length == LM_z)
                        value = va_arg(args, s64);
                    else if (flags.length == LM_t)
                        value = va_arg(args, s64);
                    else
                        value = va_arg(args, s32);

                    int c = _print_number_diouxX(buf, value, &flags);
                    buf += c;
                    break;
                }
                case 'u':
                {
                    // print an unsigned integer
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
                case 'x':
                case 'X':
                {
                    // print an unsigned integer in hexadecimal notation
                    break;
                }
                case 'o':
                {
                    // print an unsigned integer in octal notation
                    break;
                }
                case 's':
                {
                    // print string
                    char *string = va_arg(args, char *);
                    if (string)
                        putstring(pbuf, string);
                    else
                        putstring(pbuf, "(null)");
                    break;
                }
                case 'c':
                {
                    // print a character
                    char value = (char) va_arg(args, s32);
                    if (value)
                        putchar(&buf, value);
                    break;
                }
                case 'p':
                {
                    // print a pointer
                    break;
                }
                case 'a':
                case 'A':
                {
                    // print a hexadecimal number in ASCII
                    break;
                }
                case 'n':
                {
                    // print the number of characters printed
                    MOS_UNIMPLEMENTED("printf: %n");
                    break;
                }
                default:
                {
                    // C, S, m not implemented
                    MOS_ASSERT(*format == '%');
                    putchar(&buf, '%');
                    break;
                }
            }
        }
        else
        {
            putchar(&buf, *format);
        }
    }

    va_end(args);
    return buf - start;
}
