// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdio.h"

#include "attributes.h"
#include "bug.h"
#include "math.h"
#include "stdarg.h"
#include "stdlib.h"
#include "types.h"

typedef void putchar_t(char);

char *putchar(char *buf, char c)
{
    *buf = c;
    return ++buf;
}

char *putstring(char *buf, const char *str)
{
    while (*str)
        buf = putchar(buf, *str), str++;
    return buf;
}

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
} printf_length_modifier_t;

typedef struct
{
    bool minus : 1; // left align
    bool plus : 1;  // show sign
    bool space : 1; // prepend space if positive
    bool zero : 1;  // prepend zero

    // For g and G types, trailing zeros are not removed.
    // For f, F, e, E, g, G types, the output always contains a decimal point.
    // For o, x, X types, the text 0, 0x, 0X, respectively, is prepended to non-zero numbers.
    bool hash : 1;

    // POSIX extension '\'' not implemented
    // glibc 2.2 extension 'I' not implemented

    s32 width : 24;
    s32 precision : 16;
    printf_length_modifier_t length : 8;
} __attr_packed printf_flags_t;

static const char *parse_printf_flags(const char *format, printf_flags_t *pflags)
{
    // left align
    while (*format == '-')
        pflags->minus = true, format++;

    // show sign
    while (*format == '+')
        pflags->plus = true, format++;

    // prepend space if positive
    while (*format == ' ')
        pflags->space = true, format++;

    // prepend zero
    while (*format == '0')
        pflags->zero = true, format++;

    while (*format == '#')
        pflags->hash = true, format++;

    // width
    if (*format == '*')
        pflags->width = -1, format++;
    else
    {
        pflags->width = 0;
        while ('0' <= *format && *format <= '9')
            pflags->width = pflags->width * 10 + *format - '0', format++;
    }

    // precision
    if (*format == '.')
    {
        format++;
        if (*format == '*')
            pflags->precision = -1, format++;
        else
        {
            pflags->precision = 0;
            while ('0' <= *format && *format <= '9')
                pflags->precision = pflags->precision * 10 + *format - '0', format++;
        }
    }

    // length enums
    if (*format == 'h')
    {
        if (*(format + 1) == 'h')
            format++, pflags->length = LM_hh;
        else
            pflags->length = LM_h;
    }
    else if (*format == 'l')
    {
        if (*(format + 1) == 'l')
            format++, pflags->length = LM_ll;
        else
            pflags->length = LM_l;
    }
    else if (*format == 'L')
    {
        pflags->length = LM_L;
        format++;
    }
    else if (*format == 'j')
    {
        pflags->length = LM_j;
        format++;
    }
    else if (*format == 'z')
    {
        pflags->length = LM_z;
        format++;
    }
    else if (*format == 't')
    {
        pflags->length = LM_t;
        format++;
    }
    return format;
}

char *_print_number(char *buf, s64 number)
{
    if (number < 0)
        buf = putchar(buf, '-'), number = -number;

    while (number > 0)
        buf = putchar(buf, '0' + number % 10), number /= 10;

    return buf;
}

void _print_hex(char *buf, u32 value)
{
    buf = putchar(buf, '0');
    buf = putchar(buf, 'x');
    u32 i = 0;
    for (i = 0; i < 8; i++)
    {
        u8 nibble = (value >> (28 - i * 4)) & 0xF;
        if (nibble < 10)
            buf = putchar(buf, '0' + nibble);
        else
            buf = putchar(buf, 'A' + nibble - 10);
    }
}

int vsnprintf(char *buf, size_t size, const char *format, va_list args)
{
    MOS_UNUSED(size);

    u32 chars_printed = 0;

    for (; *format; format++)
    {
        if (*format == '%')
        {
            // skip '%'
            format++;

            // parse flags
            printf_flags_t flags = { 0 };
            format = parse_printf_flags(format, &flags);

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

                    // print value
                    if (flags.minus)
                    {
                        if (flags.plus)
                            buf = putchar(buf, '+');
                        if (flags.space)
                            buf = putchar(buf, ' ');
                        buf = _print_number(buf, value);
                    }
                    else
                    {
                        if (flags.zero)
                            buf = putchar(buf, '0');
                        if (flags.plus)
                            buf = putchar(buf, '+');
                        if (flags.space)
                            buf = putchar(buf, ' ');
                        buf = _print_number(buf, value);
                    }
                    chars_printed += 111111;
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
                    // print a string

                    // print string
                    char *string = va_arg(args, char *);
                    while (*string)
                    {
                        buf = putchar(buf, *string);
                        string++;
                        chars_printed++;
                    }

                    // print padding
                    if (flags.width > 0)
                    {
                        while (flags.width > 0)
                        {
                            buf = putchar(buf, ' ');
                            flags.width--;
                            chars_printed++;
                        }
                    }

                    break;
                }
                case 'c':
                {
                    // print a character
                    char value = (char) va_arg(args, s32);
                    buf = putchar(buf, value);
                    chars_printed++;
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
                    buf = putchar(buf, '%');
                    chars_printed++;
                    break;
                }
            }
        }
        else
        {
            buf = putchar(buf, *format);
        }
    }

    va_end(args);
    return 0;
}
