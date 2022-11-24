// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/x86_platform.h"

#define X86_MEM_MAX_N_REGIONS 32

extern x86_pmblock_t x86_mem_regions[X86_MEM_MAX_N_REGIONS];
extern size_t x86_mem_regions_count;
extern size_t x86_mem_size_total;
extern size_t x86_mem_available;

extern x86_pmblock_t *x86_bios_region;

void x86_mem_init(const multiboot_memory_map_t *map_entry, u32 count);
