// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/types.hpp>

typedef enum
{
    MEM_PAGETABLE, // page table pages
    MEM_SLAB,      // slab allocator
    MEM_PAGECACHE, // page cache
    MEM_KERNEL,    // kernel memory (e.g. kernel stack)
    MEM_USER,      // user memory (e.g. user code, data, stack)

    _MEM_MAX_TYPES,
} mmstat_type_t;

extern const char *mem_type_names[_MEM_MAX_TYPES];

/**
 * @brief Increment the memory usage statistics.
 *
 * @param type The type of memory.
 * @param size The size of memory.
 */
void mmstat_inc(mmstat_type_t type, size_t size);
#define mmstat_inc1(type) mmstat_inc(type, 1)

/**
 * @brief Decrement the memory usage statistics.
 *
 * @param type The type of memory.
 * @param size The size of memory.
 */
void mmstat_dec(mmstat_type_t type, size_t size);
#define mmstat_dec1(type) mmstat_dec(type, 1)

/**
 * @brief Memory usage statistics for a specific vmap area.
 *
 * @details Different types of memory:
 *  The metrics in this struct only describe what is 'mapped' in the vmap area.
 *  Unmapped pages are not counted.
 *
 *  On a page fault, the page is mapped in and the following happens:
 *
 *  Private File-backed:
 *      Read        pagecache++, cow++,
 *      Written     regular++,
 *                  if the page is already mapped, pagecache--, cow-- (page is not in the page cache)
 *      Forked      cow += regular, regular = 0 (regular pages now becomes cow pages, pagecache ones stay pagecache (read-only))
 *  Shared File-backed:
 *      Read        pagecache++, regular++,
 *      Written     if the page wasn't previously mapped, pagecache++, regular++ (a new pagecache page is now mapped)
 *  Private Anonymous:
 *      Read        cow++,
 *                  zero page is mapped
 *      Written     regular++, cow--,
 *      Forked      cow += regular, regular = 0 (regular pages now becomes cow pages)
 *  Shared Anonymous:
 *      NOT IMPLEMENTED (yet)
 *
 */
typedef struct
{
    size_t regular;   ///< regular pages with no special flags being set or unset
    size_t pagecache; ///< pages that are in the page cache (file-backed only)
    size_t cow;       ///< pages that are copy-on-write
} vmap_stat_t;

#define vmap_stat_inc(vmap, type) (vmap)->stat.type += 1
#define vmap_stat_dec(vmap, type) (vmap)->stat.type -= 1
