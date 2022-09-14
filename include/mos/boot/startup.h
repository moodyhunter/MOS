// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"

#define __startup      __section(.mos.startup.text) __attribute__((__cold__))
#define __startup_data __section(.mos.startup.data)
