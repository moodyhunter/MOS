// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

void mos_pmm_setup(void);

uintptr_t pmalloc_alloc(size_t n_pages);

void pmalloc_acquire_pages(uintptr_t start_addr, size_t npages);
void pmalloc_release_pages(uintptr_t start_addr, size_t npages);
