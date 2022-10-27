// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

void *kheap_alloc_page(size_t npages, vm_flags vmflags);
bool kheap_free_page(void *ptr, size_t npages);
