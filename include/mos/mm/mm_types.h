// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/containers.h"
#include "mos/types.h"

typedef struct
{
    as_linked_list;
    uintptr_t vaddr;
    uintptr_t paddr;
    size_t size_bytes;
    bool available;
} memblock_t;
