// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

void *kpage_alloc(size_t npages, pgalloc_hints type, vm_flags vmflags);
bool kpage_free(void *ptr, size_t npages);
