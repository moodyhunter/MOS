// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/x86/mm/paging.h"

typedef struct x86_pg_infra_t x86_pg_infra_t;

size_t pmem_freelist_size();
void pmem_freelist_dump();

void pmem_freelist_setup();
uintptr_t pmem_freelist_find_free(size_t pages);

size_t pmem_freelist_add_region(uintptr_t start_addr, size_t size_bytes);
void pmem_freelist_remove_region(uintptr_t start_addr, size_t size_bytes);
