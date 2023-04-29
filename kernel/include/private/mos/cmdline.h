// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>
#include <mos/types.h>

typedef struct
{
    const char *name;
    const char *arg;
    bool used;
} cmdline_option_t;

typedef struct
{
    const char *const name;
    bool (*setup_fn)(const char *arg);
} setup_func_t;

#define __do_setup(_param, _fn, _section)                                                                                                                                \
    static bool _fn(const char *arg);                                                                                                                                    \
    static const setup_func_t __used __setup_##_fn __section(_section) = { .name = _param, .setup_fn = _fn }

#define __setup(_param, _fn)       __do_setup(_param, _fn, ".mos.setup")
#define __early_setup(_param, _fn) __do_setup(_param, _fn, ".mos.early_setup")

extern size_t mos_cmdlines_count;
extern cmdline_option_t mos_cmdlines[MOS_MAX_CMDLINE_COUNT];

void mos_cmdline_parse(const char *bootloader_cmdline);

bool string_truthiness(const char *arg, bool default_value);

void mos_cmdline_do_setup(void);
void mos_cmdline_do_early_setup(void);
