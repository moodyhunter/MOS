// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "liballoc.h"
#include "libuserspace.h"

#include <mos/mos_global.h>
#include <mos/moslib_global.h>

MOSAPI __malloc void *malloc(size_t size);
MOSAPI void free(void *ptr);
MOSAPI void *calloc(size_t nmemb, size_t size);
MOSAPI void *realloc(void *ptr, size_t size);
