// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/platform/platform.h"

struct pagetable_do_mask_data
{
    vm_flags mask;
};

extern const pagetable_walk_options_t pagetable_do_mask_callbacks;
