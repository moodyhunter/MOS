// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

void *kmalloc(size_t size) __malloc;
void *kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(void *ptr);
