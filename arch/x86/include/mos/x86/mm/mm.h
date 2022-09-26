// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm_types.h"
#include "mos/x86/boot/multiboot.h"

#define MEM_MAX_N_REGIONS 64
extern memblock_t x86_mem_regions[MEM_MAX_N_REGIONS];
extern size_t x86_mem_regions_count;
extern size_t x86_mem_size_total;
extern size_t x86_mem_available;

void x86_mem_init(const multiboot_mmap_entry_t *map_entry, u32 count);
memblock_t *x86_mem_find_bios_block();
