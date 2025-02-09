// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/mm/paging/pml_types.hpp"

struct pagetable_do_unmap_data
{
    bool do_unref;
};

extern const pagetable_walk_options_t pagetable_do_unmap_callbacks;
