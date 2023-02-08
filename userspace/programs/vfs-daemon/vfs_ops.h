// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

enum
{
    VFSOP_OPEN,
    VFSOP_CLOSE,
    VFSOP_READ,
    VFSOP_WRITE,

    VFSOP_STAT,

    VFSOP_MAX_OP
};
