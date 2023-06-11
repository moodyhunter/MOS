// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>

typedef struct
{
    const char *name;
    const char *arg;
    bool used;
} cmdline_option_t;

extern size_t mos_cmdlines_count;
extern cmdline_option_t mos_cmdlines[MOS_MAX_CMDLINE_COUNT];

void mos_cmdline_init(const char *bootloader_cmdline);
cmdline_option_t *cmdline_get_option(const char *option_name);
bool cmdline_string_truthiness(const char *arg, bool default_value);
