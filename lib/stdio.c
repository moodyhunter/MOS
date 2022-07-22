// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdio.h"

#include "attributes.h"
#include "drivers/screen.h"
#include "stdarg.h"

typedef void putchar_t(char);

putchar_t *const putchar = &screen_print_char;

typedef enum
{
    LM_hh, // char
    LM_h,  // short
    LM_l,  // long
    LM_ll, // long long
    LM_L,  // long double
    LM_j,  // intmax_t
    LM_z,  // size_t
    LM_t,  // ptrdiff_t
} printf_length_modifier_t;

typedef struct
{
    bool minus; // left align
    bool plus;  // show sign
    bool space; // prepend space if positive
    bool zero;  // prepend zero

    // POSIX extension apostrophe not implemented
    // glibc 2.2 extension 'I' not implemented

    // For g and G types, trailing zeros are not removed.
    // For f, F, e, E, g, G types, the output always contains a decimal point.
    // For o, x, X types, the text 0, 0x, 0X, respectively, is prepended to non-zero numbers.
    bool hash;

    s32 width;
    s32 precision;
    printf_length_modifier_t length;
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

void screen_print_decimal(u32 value)
{
    if (value == 0)
    {
        putchar('0');
        return;
    }
    u32 divisor = 1000000000;
    u32 remainder = value;
    while (divisor > 0)
    {
        u32 digit = remainder / divisor;
        remainder %= divisor;
        if (digit > 0 || divisor == 1)
        {
            putchar('0' + digit);
        }
        divisor /= 10;
    }
}

void screen_print_hex(u32 value)
{
    screen_print_string("0");
    screen_print_string("x");
    u32 i = 0;
    for (i = 0; i < 8; i++)
    {
        u8 nibble = (value >> (28 - i * 4)) & 0xF;
        if (nibble < 10)
            screen_print_char('0' + nibble);
        else
            screen_print_char('A' + nibble - 10);
    }
}

int printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);

    for (; *format; format++)
    {
        if (*format == '%')
        {
            format++; // skip '%'

            // parse flags
            printf_flags_t flags;
            format = parse_printf_flags(format, &flags);

            switch (*format)
            {
                case '%':
                {
                    // print '%'
                    break;
                }
                case 'd':
                case 'i':
                {
                    // print a signed integer
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
                    break;
                }
                case 'c':
                {
                    // print a character
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
                    break;
                }
                default:
                {
                    // C, S, m not implemented
                    break;
                }
            }
        }
        else
        {
            screen_print_char(*format);
        }
    }

    va_end(args);
    return 0;
}
