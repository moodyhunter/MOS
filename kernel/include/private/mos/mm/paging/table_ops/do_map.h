// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/platform/platform.h"

struct pagetable_do_map_data
{
    pfn_t pfn;
    vm_flags flags;
};

extern const pagetable_walk_options_t pagetable_do_map_callbacks;
