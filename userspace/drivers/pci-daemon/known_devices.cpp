// SPDX-License-Identifier: GPL-3.0-or-later

#include "known_devices.hpp"

#include <format>
#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct
{
    u8 base_class;
    u8 sub_class;
    u8 prog_if;
    const std::string_view name;
} known_classes[] = {
#define _X(x, y, z, name) { x, y, z, name },
#include "builtin.hpp"
#undef _X
};

std::string get_known_class_name(u8 base_class, u8 sub_class, u8 prog_if)
{
    for (size_t i = 0; i < MOS_ARRAY_SIZE(known_classes); i++)
    {
        const u8 k_baseclass = known_classes[i].base_class;
        const u8 k_subclass = known_classes[i].sub_class;
        const u8 k_progif = known_classes[i].prog_if;

        if (k_baseclass == base_class && k_subclass == sub_class && k_progif == prog_if)
            return std::string{ known_classes[i].name };
    }

    return std::format("Unknown class: {:02x}:{:02x}:{:02x}", base_class, sub_class, prog_if);
}
