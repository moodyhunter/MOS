// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"
#include "mos/x86/boot/multiboot.h"

void x86_mem_init(const multiboot_memory_map_t *map_entry, u32 count);
