// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "lib/stdlib.h"
#include "mos/mm/mm_types.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#define MEM_MAX_BLOCKS 64

static memblock_t mem_regions[MEM_MAX_BLOCKS] = { 0 };

static size_t mem_available_count = 0;
static bool mem_initialized = false;

void mos_mem_add_region(u64 start, size_t size, bool available)
{
    MOS_ASSERT(!mem_initialized);
    if (mem_available_count == MEM_MAX_BLOCKS)
        mos_panic("too many available regions added.");

    memblock_t *block = &mem_regions[mem_available_count];
    block->addr = start;
    block->size = size;
    block->available = available;
    mem_available_count++;
}

void mos_mem_finish_setup()
{
    MOS_ASSERT_X(!mem_initialized, "memory already initialized.");
    mem_initialized = true;

    u64 mem_size_total = 0, mem_size_avail = 0;
    uintptr_t page_begin, page_end;

    const u32 page_size = mos_platform.mm_page_size;
    for (size_t i = 0; i < mem_available_count; i++)
    {
        mem_size_total += mem_regions[i].size;

        if (!mem_regions[i].available)
            continue;

        const memblock_t *region = &mem_regions[i];

        mem_size_avail += region->size;
        page_begin = region->addr / page_size;

        u64 range_end_addr = region->addr + region->size;

        // we only support full pages
        if ((region->addr % page_size) != 0)
            page_begin += 1;

        page_end = range_end_addr / page_size;

        if ((range_end_addr % page_size) == 0)
            page_end -= 1;
    }

#define SIZE_BUF_LEN 8
    char buf[SIZE_BUF_LEN];
    char buf_available[SIZE_BUF_LEN];
    char buf_unavailable[SIZE_BUF_LEN];
    format_size(buf, sizeof(buf), mem_size_total);
    format_size(buf_available, sizeof(buf_available), mem_size_avail);
    format_size(buf_unavailable, sizeof(buf_unavailable), mem_size_total - mem_size_avail);
    pr_info("Total Memory: %s (%s available, %s unavailable)", buf, buf_available, buf_unavailable);
}
