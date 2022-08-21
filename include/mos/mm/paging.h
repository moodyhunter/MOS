// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

void *kpage_alloc(size_t npages);

// 'ptr' is the value returned from a previous mm_page_alloc call.
bool kpage_free(void *ptr, size_t npages);
