// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/kallsyms.h>

const kallsyms_t *kallsyms_get_symbol_name(ptr_t addr)
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
