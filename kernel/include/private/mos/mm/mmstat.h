// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/types.h>

typedef enum
{
    MEM_PAGETABLE, // page table pages
    MEM_SLAB,      // slab allocator
    MEM_KERNEL,    // kernel memory (e.g. kernel stack)
    MEM_USER,      // user memory (e.g. user code, data, stack)

    _MEM_MAX_TYPES,
} mem_type_t;

extern const char *mem_type_names[_MEM_MAX_TYPES];

/**
 * @brief Increment the memory usage statistics.
 *
 * @param type The type of memory.
 * @param size The size of memory.
 */
void mmstat_inc(mem_type_t type, size_t size);
#define mmstat_inc1(type) mmstat_inc(type, 1)

/**
 * @brief Decrement the memory usage statistics.
 *
 * @param type The type of memory.
 * @param size The size of memory.
 */
void mmstat_dec(mem_type_t type, size_t size);
#define mmstat_dec1(type) mmstat_dec(type, 1)
