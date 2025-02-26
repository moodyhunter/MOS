// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/platform/platform.hpp"

struct pagetable_do_flag_data
{
    VMFlags flags;
};

extern const pagetable_walk_options_t pagetable_do_flag_callbacks;
