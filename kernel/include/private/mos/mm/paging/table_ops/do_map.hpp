// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/platform/platform.hpp"

struct pagetable_do_map_data
{
    pfn_t pfn;
    VMFlags flags;
    bool do_refcount; // whether to increment the reference count of the frame
};

extern const pagetable_walk_options_t pagetable_do_map_callbacks;
