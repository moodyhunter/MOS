// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.hpp"

#include <mos/allocator.hpp>

extern mos::Slab<u8> xsave_area_slab;

void x86_xsave_thread(Thread *thread);
void x86_xrstor_thread(Thread *thread);
