// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

void mos_pmem_freelist_setup();

size_t pmem_freelist_size();
void pmem_freelist_dump();

uintptr_t pmem_freelist_allocate_free(size_t pages);

size_t pmem_freelist_add_region(uintptr_t start_addr, size_t size_bytes);
void pmem_freelist_remove_region(uintptr_t start_addr, size_t size_bytes);
