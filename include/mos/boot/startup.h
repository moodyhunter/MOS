// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"

#define __startup_code   __section(.mos.startup.text)
#define __startup_rodata __section(.mos.startup.rodata)
#define __startup_rwdata __section(.mos.startup.data)
