// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

void mos_pmm_setup(void);

void pmalloc_dump(void);

uintptr_t pmalloc_alloc(size_t n_pages);

void pmalloc_acquire_region(uintptr_t start_addr, size_t size_bytes);
size_t pmalloc_release_region(uintptr_t start_addr, size_t size_bytes);
