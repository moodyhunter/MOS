// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

#include KERNEL_INTERNAL_CHECK

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
        const char *string;
        bool boolean;
    } val;
} cmdline_param_t;

typedef struct
{
    const char *arg_name;
    size_t params_count;
    cmdline_param_t **params;
} cmdline_arg_t;

typedef struct
{
    size_t args_count;
    cmdline_arg_t **arguments;
} cmdline_t;

extern cmdline_t *mos_cmdline;

cmdline_t *mos_cmdline_create(const char *kcmdline);
void mos_cmdline_destroy(cmdline_t *cmdline);

cmdline_arg_t *mos_cmdline_get_arg(const char *arg);
