// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef enum
{
    CMDLINE_PARAM_TYPE_STRING,
    CMDLINE_PARAM_TYPE_BOOL,
} option_value_type_t;

typedef struct
{
    option_value_type_t param_type;
    union
    {
        char *string;
        bool boolean;
    } val;
} cmdline_parameter_t;

typedef struct
{
    char *name;
    size_t parameters_count;
    cmdline_parameter_t *parameters[];
} cmdline_option_t;

typedef struct
{
    size_t options_count;
    cmdline_option_t *options[];
} cmdline_t;

cmdline_t *parse_cmdline(const char *kcmdline);
cmdline_option_t *cmdline_get_option(cmdline_t *cmdline, const char *name);
