// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct uuid
{
    u32 time_low;
    u16 time_mid;
    u16 time_hi_and_version;
    u8 clock_seq_hi_and_reserved;
    u8 clock_seq_low;
    u8 node[6];
} uuid_t;

static_assert(sizeof(uuid_t) == 16, "uuid_t is not 16 bytes");
