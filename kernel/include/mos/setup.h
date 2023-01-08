// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/cmdline.h"
#include "mos/platform/platform.h"

typedef struct
{
    const char *const param;
    bool (*setup_fn)(int argc, const char **argv);
} setup_func_t;

#define __setup(_name, _param_name, _initfn) const setup_func_t __setup_##_name __section(".mos.setup") = { .param = _param_name, .setup_fn = _initfn }

void invoke_setup_functions(cmdline_t *cmdline);
