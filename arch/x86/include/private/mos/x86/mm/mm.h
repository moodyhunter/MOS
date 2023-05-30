// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.h>
#include <mos/x86/boot/multiboot.h>

void x86_pmm_region_setup(const multiboot_memory_map_t *map_entry, u32 count, ptr_t initrd_paddr, size_t initrd_size);
