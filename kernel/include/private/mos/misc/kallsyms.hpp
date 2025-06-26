// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.hpp>

typedef struct
{
    ptr_t address;
    const char *name;
} kallsyms_t;

extern const kallsyms_t mos_kallsyms[];

#define mos_caller() (kallsyms_get_symbol_name((ptr_t) __builtin_return_address(0)))

const kallsyms_t *kallsyms_get_symbol(ptr_t addr);
const char *kallsyms_get_symbol_name(ptr_t addr);
ptr_t kallsyms_get_symbol_address(mos::string_view name);
