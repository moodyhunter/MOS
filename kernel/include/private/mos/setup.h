// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/cmdline.h"

typedef struct
{
    const char *const name;
    bool (*setup_fn)(int argc, const char **argv);
} setup_func_t;

#define __setup(_fn, _param) static const setup_func_t __used __setup_##_fn __section(".mos.setup") = { .name = _param, .setup_fn = _fn }

void invoke_setup_functions(cmdline_t *cmdline);
