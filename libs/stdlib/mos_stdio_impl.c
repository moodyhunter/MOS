// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mos_global.h>
#include <mos/moslib_global.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

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
    bool left_aligned;      // -
    bool show_sign;         // +
    bool space_if_positive; // prepend space if positive
    bool pad_with_zero;     // padding with zero

    // For g and G types, trailing zeros are not removed.
    // For f, F, e, E, g, G types, the output always contains a decimal point.
    // For o, x, X types, the text 0, 0x, 0X, respectively, is prepended to non-zero numbers.
    bool hash;

    // POSIX extension '\'' not implemented
    // glibc 2.2 extension 'I' not implemented

    // If the converted value has fewer characters than the field width, it will be padded with spaces on the left
    // (or right, if the left-adjustment flag has been given).
    // A negative field width is taken as a '-' flag followed by a positive field width.
    // In no case does a nonexistent or small field width cause truncation of a field;
    // if the result of a conversion is wider than the field width, the field is expanded to contain the conversion result.
    s32 minimum_width;

    bool has_explicit_precision;
    // `d, i, o, u, x, X` -> The minimum number of digits to appear;
    // `a, A, e, E, f, F` -> The number of digits to appear after the radix character;
    // `g, G`             -> The maximum number of significant digits;
    // `s, S`             -> The maximum number of characters to be printed from a string;
    s32 precision;

    length_modifier_t length;
} printf_flags_t;

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
        {
#if MOS_LP32
            pflags->length = LM__l;
#elif MOS_LP64
            pflags->length = LM_ll; // 64-bit long
#else
#error "Unknown long size"
#endif
        }
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
should_inline __nodiscard int buf_putchar(char *buf, char c, size_t *size_left)
{
    if (*size_left > 0)
    {
        *buf = c;
        (*size_left)--;
    }

    return 1;
}

static const char *const lower_hex_digits = "0123456789abcdef";
static const char *const upper_hex_digits = "0123456789ABCDEF";

#define wrap_printed(x)                                                                                                                                                  \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        const size_t _printed = x;                                                                                                                                       \
        buf += _printed, ret += _printed;                                                                                                                                \
    } while (0)

// ! prints d, i, o, u, x, and X
static int printf_diouxX(char *buf, u64 number, printf_flags_t *pflags, char conv, size_t *size_left)
{
    size_t ret = 0;
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

    enum
    {
        BASE_8 = 8,
        BASE_10 = 10,
        BASE_16 = 16,
    } base = BASE_10;
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

    char num_prefix_buf[5] = { 0 };
    char num_content_buf[32] = { 0 };
    size_t num_content_len = 32;

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
                    pnumberbuf += buf_putchar(pnumberbuf, '0' + (char) (number % base), &num_content_len), number /= base;
                break;
            case BASE_16:
                while (number > 0)
                    pnumberbuf += buf_putchar(pnumberbuf, hex_digits[number % 16], &num_content_len), number /= 16;
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
    s32 width_to_pad = MAX(pflags->minimum_width - strlen(num_prefix_buf) - precision_padding - n_digits, 0u);

    char *pnum_prefix = num_prefix_buf;
    char *pnum_content = num_content_buf + n_digits;
    if (pflags->left_aligned)
    {
        while (*pnum_prefix)
            wrap_printed(buf_putchar(buf, *pnum_prefix++, size_left));
        while (precision_padding-- > 0)
            wrap_printed(buf_putchar(buf, '0', size_left));
        while (pnum_content > num_content_buf)
            wrap_printed(buf_putchar(buf, *--pnum_content, size_left));
        while (width_to_pad-- > 0)
            wrap_printed(buf_putchar(buf, ' ', size_left));
    }
    else
    {
        if (pflags->pad_with_zero)
        {
            // zero should be after the sign
            while (*pnum_prefix)
                wrap_printed(buf_putchar(buf, *pnum_prefix++, size_left));
            while (width_to_pad-- > 0)
                wrap_printed(buf_putchar(buf, '0', size_left));
        }
        else
        {
            // space should be before the sign
            while (width_to_pad-- > 0)
                wrap_printed(buf_putchar(buf, ' ', size_left));
            while (*pnum_prefix)
                wrap_printed(buf_putchar(buf, *pnum_prefix++, size_left));
        }
        while (precision_padding-- > 0)
            wrap_printed(buf_putchar(buf, '0', size_left));
        while (pnum_content > num_content_buf)
            wrap_printed(buf_putchar(buf, *--pnum_content, size_left));
    }
    return ret;
}

static int printf_cs(char *buf, const char *data, printf_flags_t *pflags, char conv, size_t *psize_left)
{
    size_t ret = 0;
    if (data == NULL)
        data = "(null)";
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

    s32 printed_len = conv == 'c' ? 1 : strlen(data);

    // If presition is given, the string is truncated to this length.
    if (pflags->has_explicit_precision)
        printed_len = MIN(printed_len, pflags->precision);

    s32 width_to_pad = MAX(pflags->minimum_width - printed_len, 0);

    if (pflags->left_aligned)
    {
        while (printed_len-- > 0)
            wrap_printed(buf_putchar(buf, *data++, psize_left));
        while (width_to_pad-- > 0)
            wrap_printed(buf_putchar(buf, ' ', psize_left));
    }
    else
    {
        while (width_to_pad-- > 0)
            wrap_printed(buf_putchar(buf, ' ', psize_left));
        while (printed_len-- > 0)
            wrap_printed(buf_putchar(buf, *data++, psize_left));
    }

    return ret;
}

int vsnprintf(char *buf, size_t size, const char *format, va_list _args)
{
    size_t ret = 0;
    if (size == 0)
        return 0; // nothing to do

    va_list_ptrwrappper_t args;
    va_copy(args.real, _args);

    for (; *format; format++)
    {
        if (*format != '%')
        {
            wrap_printed(buf_putchar(buf, *format, &size));
            continue;
        }

        // skip '%'
        format++;

        // parse flags
        printf_flags_t flags = { 0 };
        format += parse_printf_flags(format, &flags, &args);

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
                wrap_printed(printf_diouxX(buf, value, &flags, *format, &size));
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
                const char *string = va_arg(args.real, char *);
                wrap_printed(printf_cs(buf, string, &flags, *format, &size));
                break;
            }
            case 'c':
            {
                // print a character
                char value = (char) va_arg(args.real, s32);
                wrap_printed(printf_cs(buf, &value, &flags, *format, &size));
                break;
            }
            case 'p':
            {
                // print a pointer
                ptr_t value = (ptr_t) va_arg(args.real, void *);
#ifdef __MOS_KERNEL__
                // print special kernel data types
                extern size_t vsnprintf_do_pointer_kernel(char *buf, size_t *size, const char **pformat, ptr_t ptr);
                const size_t siz = vsnprintf_do_pointer_kernel(buf, &size, &format, value);
                wrap_printed(siz);
                if (siz)
                    break;
#endif
                wrap_printed(buf_putchar(buf, '0', &size));
                wrap_printed(buf_putchar(buf, 'x', &size));
                flags.length = MOS_BITS == 32 ? LM__l : LM_ll;
                wrap_printed(printf_diouxX(buf, value, &flags, 'x', &size));
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
                wrap_printed(buf_putchar(buf, '%', &size));
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
                wrap_printed(buf_putchar(buf, '%', &size));
                wrap_printed(buf_putchar(buf, *format, &size));
                break;
            }
        }
    }
end:
    buf += buf_putchar(buf, 0, &size);

    va_end(args.real);
    return ret;
}
