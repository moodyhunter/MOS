// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/containers.h"

#include <stddef.h>

typedef struct
{
    u64 addr;
    size_t size;
    bool available;
} memblock_t;
