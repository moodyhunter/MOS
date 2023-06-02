// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/cmdline.h>
#include <stdlib.h>
#include <string.h>

// (result array, cmdline, max cmdlines) -> a new result array
typedef const char **(*cmdline_insert_fn_t)(const char **result, size_t result_capacity, char *cmdline, size_t *result_count);

static const char **cmdline_static_array_insert(const char **result, size_t result_capacity, char *cmdline, size_t *result_count)
{
    if (*result_count == result_capacity)
        return NULL;

    result[*result_count] = cmdline;
    (*result_count)++;
    return result;
}

static const char **cmdline_dynamic_array_insert(const char **argv, size_t result_capacity, char *cmdline, size_t *result_count)
{
    MOS_UNUSED(result_capacity); // unused because we always realloc
    argv = realloc(argv, sizeof(char *) * (*result_count + 1));
    argv[*result_count] = strdup(cmdline);
    (*result_count)++;
    return argv;
}

static bool cmdline_parse_generic(char *start, size_t length, size_t cmdline_max, size_t *out_count, const char ***argv_ptr, cmdline_insert_fn_t insert)
{
    char *buf_start = start;

    if (length == 0)
        return true;

    // replace all spaces with null terminators, except for the ones quoted, and also handle escaped quotes
    bool escaped = false;
    enum
    {
        Q_SINGLE,
        Q_DOUBLE,
        Q_NONE,
    } quote_type = Q_NONE;

    for (char *c = start; c != &buf_start[length + 1]; c++)
    {
        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (*c == '\\')
        {
            escaped = true;
            continue;
        }

        if (*c == '\'' && quote_type != Q_DOUBLE)
        {
            quote_type = quote_type == Q_SINGLE ? Q_NONE : Q_SINGLE;
            continue;
        }

        if (*c == '"' && quote_type != Q_SINGLE)
        {
            quote_type = quote_type == Q_DOUBLE ? Q_NONE : Q_DOUBLE;
            continue;
        }

        if (*c == ' ' && quote_type == Q_NONE)
            *c = '\0';

        if (*c == '\0' && quote_type != Q_NONE)
            *c = ' '; // we are in a quoted string, so replace null terminator with space

        if (*c == '\0')
        {
            if (strlen(start) > 0)
            {
                *argv_ptr = insert(*argv_ptr, cmdline_max, start, out_count);
                if (!*argv_ptr)
                    return false;
            }

            start = c + 1;
        }
    }

    return true;
}

bool cmdline_parse_inplace(char *inbuf, size_t length, size_t cmdline_max, size_t *out_count, const char **out_cmdlines)
{
    return cmdline_parse_generic(inbuf, length, cmdline_max, out_count, &out_cmdlines, cmdline_static_array_insert);
}

const char **cmdline_parse(const char **inargv, char *inbuf, size_t length, size_t *out_count)
{
    if (!cmdline_parse_generic(inbuf, length, 0, out_count, &inargv, cmdline_dynamic_array_insert))
    {
        return NULL;
    }
    return inargv;
}

void string_unquote(char *str)
{
    size_t len = strlen(str);
    if (len < 2)
        return;

    char quote = str[0];
    if (quote != '\'' && quote != '"')
        return;

    if (str[len - 1] != quote)
        return; // unbalanced quotes

    for (size_t i = 1; i < len - 1; i++)
    {
        if (str[i] == '\\')
        {
            if (str[i + 1] == quote)
            {
                memmove(&str[i], &str[i + 1], len - i);
                len--;
            }
            else if (str[i + 1] == '\\')
            {
                memmove(&str[i], &str[i + 1], len - i);
                len--;
            }
        }
    }

    str[len - 1] = '\0';
    memmove(str, &str[1], len - 1);
}
