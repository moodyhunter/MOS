// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.h"

extern slab_t *xsave_area_slab;

void x86_xsave_current();
void x86_xrstor_current();
