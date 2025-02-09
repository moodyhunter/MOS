// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.hpp>

typedef struct
{
    const char *name;
    const char *arg;
    bool used;
} cmdline_option_t;

void mos_cmdline_init(const char *bootloader_cmdline);
cmdline_option_t *cmdline_get_option(const char *option_name);
bool cmdline_string_truthiness(const char *arg, bool default_value);
