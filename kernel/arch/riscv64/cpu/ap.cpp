// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syslog/syslog.hpp"

#include <mos/types.h>

void platform_ap_entry(u64 id)
{
    mInfo << "AP " << id << " started";
}
