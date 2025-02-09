// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.hpp"
#include "mos/platform/platform.hpp"

extern slab_t *xsave_area_slab;

void x86_xsave_thread(thread_t *thread);
void x86_xrstor_thread(thread_t *thread);
