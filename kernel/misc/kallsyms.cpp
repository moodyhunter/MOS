// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/kallsyms.hpp"

const kallsyms_t *kallsyms_get_symbol(ptr_t addr)
{
    // kallsyms are sorted by address
    const kallsyms_t *ks = mos_kallsyms;
    const kallsyms_t *result = NULL;

    while (ks->name)
    {
        if (ks->address > addr)
            break;
        result = ks;
        ks++;
    }

    return result;
}

const char *kallsyms_get_symbol_name(ptr_t addr)
{
    const kallsyms_t *ks = kallsyms_get_symbol(addr);
    return ks ? ks->name : "<unknown>";
}

ptr_t kallsyms_get_symbol_address(mos::string_view name)
{
    const kallsyms_t *ks = mos_kallsyms;
    while (ks->name)
    {
        if (name == ks->name)
            return ks->address;
        ks++;
    }
    return 0; // Not found
}
