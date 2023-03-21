// SPDX-License-Identifier: BSD-3-Clause
#pragma once

/**  Durand's Amazing Super Duper Memory functions.  */

#include <mos/types.h>

void liballoc_dump(void);
void liballoc_init(void);
void *liballoc_malloc(size_t size);
void *liballoc_realloc(void *ptr, size_t size);
void *liballoc_calloc(size_t object, size_t n_objects);
void liballoc_free(const void *ptr);

void *liballoc_alloc_page(size_t npages);
bool liballoc_free_page(void *vptr, size_t npages);
