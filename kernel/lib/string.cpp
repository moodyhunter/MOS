// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/string.hpp"

#include <mos_stdio.hpp>

namespace mos
{
    mos::string to_string(const void *value)
    {
        char buf[32];
        const auto sz = snprintf(buf, sizeof(buf), "%p", value);
        return mos::string(buf, sz);
    }
} // namespace mos
