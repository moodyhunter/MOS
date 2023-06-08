// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>
#include <mos/x86/boot/multiboot.h>

/**
 * @brief Physical page number of the phyframes array.
 *
 */
extern pfn_t phyframes_pfn;

/**
 * @brief Number of pages required for the phyframes array.
 *
 */
extern size_t phyframes_npages;

void x86_pmm_region_setup(const multiboot_memory_map_t *map_entry, u32 count, pfn_t initrd_pfn, size_t initrd_size);
