// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

#include MOS_KERNEL_INTERNAL_HEADER_CHECK

typedef struct
{
    const char *name;
    u32 argc;
    const char **argv;
} cmdline_option_t;

typedef struct
{
    size_t options_count;
    cmdline_option_t **options;
} cmdline_t;

extern cmdline_t *mos_cmdline;

cmdline_t *cmdline_create(const char *kcmdline);
bool cmdline_remove_option(cmdline_t *cmdline, const char *arg);
void cmdline_destroy(cmdline_t *cmdline);
