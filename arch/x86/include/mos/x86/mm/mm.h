// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"
#include "mos/x86/boot/multiboot.h"

void x86_setup_mem(multiboot_mmap_entry_t *map_entry, u32 count);
