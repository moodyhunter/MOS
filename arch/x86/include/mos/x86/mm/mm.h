// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm_types.h"
#include "mos/x86/boot/multiboot.h"

#define X86_MEM_MAX_N_REGIONS 32

extern memblock_t x86_mem_regions[X86_MEM_MAX_N_REGIONS];
extern size_t x86_mem_regions_count;
extern size_t x86_mem_size_total;
extern size_t x86_mem_available;

extern memblock_t *x86_bios_region;

void x86_mem_init(const multiboot_memory_map_t *map_entry, u32 count);
