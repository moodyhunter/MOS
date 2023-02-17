// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/liballoc.h"
#include "libuserspace.h"
#include "mos/mos_global.h"

__mosapi __malloc void *malloc(size_t size);
__mosapi void free(void *ptr);
__mosapi void *calloc(size_t nmemb, size_t size);
__mosapi void *realloc(void *ptr, size_t size);
