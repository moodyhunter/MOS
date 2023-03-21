// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <mos/types.h>

typedef struct
{
    uintptr_t address;
    const char *name;
} kallsyms_t;

extern const kallsyms_t mos_kallsyms[];

const kallsyms_t *kallsyms_get_symbol_name(uintptr_t addr);
