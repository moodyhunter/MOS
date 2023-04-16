// SPDX-License-Identifier: GPL-3.0-or-later

#include "string.h"

#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>

static const struct
{
    u8 base_class;
    u8 sub_class;
    u8 prog_if;
    const char *name;
} known_classes[] = {
#define _X(x, y, z, name) { 0x##x, 0x##y, 0x##z, name },
#include "builtin.h"
#undef _X
};

const char *get_known_class_name(u8 base_class, u8 sub_class, u8 prog_if)
{
    for (size_t i = 0; i < MOS_ARRAY_SIZE(known_classes); i++)
    {
        const u8 k_baseclass = known_classes[i].base_class;
        const u8 k_subclass = known_classes[i].sub_class;
        const u8 k_progif = known_classes[i].prog_if;

        if (k_baseclass == base_class && k_subclass == sub_class && k_progif == prog_if)
            return strdup(known_classes[i].name);
    }

    char *name = malloc(64);
    sprintf(name, "Unknown class: %02x:%02x:%02x", base_class, sub_class, prog_if);
    return name;
}
