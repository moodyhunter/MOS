// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/containers.h"
#include "mos/types.h"

typedef struct
{
    as_linked_list;
    u64 vaddr;
    u64 paddr;
    size_t size;
    bool available;
} memblock_t;
