// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"
#include "mos/x86/boot/multiboot.h"

extern const char __MOS_X86_PAGING_AREA_START;
extern const char __MOS_X86_PAGE_TABLE_START;
extern const char __MOS_X86_PAGING_AREA_END;

void x86_setup_mm(multiboot_mmap_entry_t *map_entry, u32 count);
