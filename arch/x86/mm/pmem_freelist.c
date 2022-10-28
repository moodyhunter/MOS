// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/pmem_freelist.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/printk.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_platform.h"

#define RESERVED_LOMEM 1 MB

#define PMEM_FREELIST_SIZE_FOR(mem_size) (((mem_size) / 2 / MOS_PAGE_SIZE) * sizeof(pmem_range_t))

typedef struct pmem_range_t
{
    struct pmem_range_t *next;
    uintptr_t paddr;
    size_t n_pages;
} pmem_range_t;

extern const void __MOS_X86_PMEM_FREE_LIST;
static pmem_range_t *const pmem_freelist_storage = (pmem_range_t *) &__MOS_X86_PMEM_FREE_LIST;
static pmem_range_t *pmem_freelist = (pmem_range_t *) &__MOS_X86_PMEM_FREE_LIST;
static size_t pmem_freelist_count = 0;

size_t pmem_freelist_size()
{
    return ALIGN_UP_TO_PAGE(PMEM_FREELIST_SIZE_FOR(x86_mem_available));
}

void pmem_freelist_dump()
{
    pr_info("pmem freelist has %zu entries", pmem_freelist_count);
    pmem_range_t *this = pmem_freelist;
    while (this)
    {
        const uintptr_t pstart = this->paddr;
        const uintptr_t pend = pstart + this->n_pages * MOS_PAGE_SIZE;
        char sbuf[32];
        format_size(sbuf, sizeof(sbuf), this->n_pages * MOS_PAGE_SIZE);
        pr_info("  [" PTR_FMT "] entry: " PTR_FMT "-" PTR_FMT " (%zu page(s), %s)", (uintptr_t) this, pstart, pend, this->n_pages, sbuf);
        this = this->next;
    }
}

void pmem_freelist_setup(void)
{
    size_t list_size = pmem_freelist_size();
    pr_info2("%zu bytes (aligned) required for physical memory freelist", list_size);

    memset(pmem_freelist, 0, list_size);

    // add current physical memory region to the freelist
    for (size_t i = 0; i < x86_mem_regions_count; i++)
    {
        memblock_t *r = &x86_mem_regions[i];
        if (!r->available)
            continue;

        if (r->paddr + r->size_bytes < RESERVED_LOMEM)
        {
            pr_emph("ignored low memory: " PTR_FMT "-" PTR_FMT " (%zu bytes)", r->paddr, r->paddr + r->size_bytes, r->size_bytes);
            continue;
        }

        size_t alignment_loss = pmem_freelist_add_region(r->paddr, r->size_bytes);
        if (alignment_loss)
            pr_emph("%zu bytes of memory loss due to alignment", alignment_loss);
    }
}

#define this_start (this->paddr)
#define this_end   (this_start + this->n_pages * MOS_PAGE_SIZE)

size_t pmem_freelist_add_region(uintptr_t start_addr, size_t size_bytes)
{
    const uintptr_t aligned_start = ALIGN_UP_TO_PAGE(start_addr);
    const uintptr_t aligned_end = ALIGN_DOWN_TO_PAGE(start_addr + size_bytes);

    const size_t pages_in_region = (aligned_end - aligned_start) / MOS_PAGE_SIZE;

    mos_debug("adding physical memory region " PTR_FMT "-" PTR_FMT " to freelist.", aligned_start, aligned_end);

    pmem_range_t *this = pmem_freelist;
    pmem_range_t *prev = NULL;

    while (this)
    {
        // * if the aligned start/end address is within the current region, it overlaps
        if ((this_start <= aligned_start && aligned_start < this_end) || (this_start < aligned_end && aligned_end <= this_end))
        {
            mos_panic("new pmem " PTR_FMT "-" PTR_FMT " overlaps with " PTR_FMT "-" PTR_FMT, aligned_start, aligned_end, this_start, this_end);
            return 0;
        }

        // prepend to 'this' region
        if (aligned_end == this_start)
        {
            uintptr_t new_end = this_start + pages_in_region * MOS_PAGE_SIZE;
            mos_debug("enlarge range [" PTR_FMT "-" PTR_FMT "]: starts at " PTR_FMT, this_start, this_end, new_end);
            this->paddr = aligned_start;
            this->n_pages += pages_in_region;
            goto end;
        }

        // append to the previous region (if possible)
        if (prev)
        {
            // * the new region "may" overlap with next region
            // * if we 'append' to the current region, we will not be able to check for such overlap
            // * (because we would have 'goto end'-ed)
            const uintptr_t prev_start = prev->paddr;
            const uintptr_t prev_end = prev_start + prev->n_pages * MOS_PAGE_SIZE;

            if (aligned_start == prev_end)
            {
                prev->n_pages += pages_in_region;
                const uintptr_t new_ends = prev_end + pages_in_region * MOS_PAGE_SIZE;
                mos_debug("enlarged " PTR_FMT "-" PTR_FMT ": ends at " PTR_FMT, prev_start, prev_end, new_ends);
                goto end;
            }
        }

        // same as above, we only 'prepend' the region before 'this'
        if (aligned_end < this_start)
        {
            pmem_range_t *new_range = &pmem_freelist_storage[pmem_freelist_count++];
            new_range->paddr = aligned_start;
            new_range->n_pages = pages_in_region;
            new_range->next = this;
            if (prev)
                prev->next = new_range;
            else
                pmem_freelist = new_range;
            goto end;
        }

        prev = this;
        this = this->next;
    }

    MOS_ASSERT(this == NULL && prev != NULL);
    pmem_range_t *new_range = &pmem_freelist_storage[pmem_freelist_count++];
    prev->next = new_range;

    // the freelist is in the correct order, so we can insert the new range here
    new_range->paddr = aligned_start;
    new_range->n_pages = pages_in_region;
    new_range->next = NULL;

end:
    // return the amount of memory that was lost due to alignment
    return (aligned_start - start_addr) + ((start_addr + size_bytes) - aligned_end);
}

void pmem_freelist_remove_region(uintptr_t start_addr, size_t size_bytes)
{
    const uintptr_t end_addr = start_addr + size_bytes;

    // the addresses must be aligned to page size
    MOS_ASSERT(start_addr % MOS_PAGE_SIZE == 0);
    MOS_ASSERT(end_addr % MOS_PAGE_SIZE == 0);

    const size_t pages_in_region = (end_addr - start_addr) / MOS_PAGE_SIZE;

    mos_debug("removing physical memory region " PTR_FMT "-" PTR_FMT " from freelist.", start_addr, end_addr);

    bool needs_cleanup = false;
    bool freed = false;
    void *cleanup_target_memptr = NULL;

    pmem_range_t *this = pmem_freelist;
    pmem_range_t *prev = NULL;
    while (this)
    {
        pmem_range_t *next = this->next;
        if (this_start <= start_addr && this_end >= end_addr)
        {
            // starts before this region
            if (start_addr < this_start)
                continue;

            // or ends after this region
            if (end_addr > this_end)
                continue;

            const size_t part_1_size = start_addr - this_start;
            const size_t part_2_size = this_end - end_addr;

            if (part_1_size == 0 && part_2_size != 0)
            {
                // part 1 is empty, which means we are removing from the front of the region
                this->paddr = end_addr;
                this->n_pages = part_2_size / MOS_PAGE_SIZE;
                mos_debug("pmem block now starts at " PTR_FMT ", with %zu pages", this_start, this->n_pages);
            }
            else if (part_1_size != 0 && part_2_size == 0)
            {
                // part 2 is empty, which means we are removing the tail of part 1
                mos_debug("shrunk " PTR_FMT "-" PTR_FMT ": ends at " PTR_FMT, this_start, this_end, this_start + (this->n_pages - pages_in_region) * MOS_PAGE_SIZE);
                this->n_pages -= pages_in_region;
            }
            else if (part_1_size == 0 && part_2_size == 0)
            {
                // part 1 and part 2 are empty, so remove this entry
                if (prev)
                    prev->next = next;
                else
                    pmem_freelist = next;

                // have to do some cleanup
                needs_cleanup = true;
                cleanup_target_memptr = this;
                mos_debug("removed " PTR_FMT "-" PTR_FMT " from freelist.", this_start, this_end);
            }
            else
            {
                // neither part 1 nor part 2 is empty, so we have to allocate a new entry for part 2
                mos_debug("split " PTR_FMT "-" PTR_FMT " into " PTR_FMT "-" PTR_FMT " and " PTR_FMT "-" PTR_FMT, this_start, this_end, this_start, start_addr, end_addr,
                          end_addr + part_2_size);
                pmem_range_t *new = (pmem_range_t *) pmem_freelist_storage + pmem_freelist_count++;
                memset(new, 0, sizeof(pmem_range_t));
                new->next = next;
                this->next = new;
                this->n_pages = part_1_size / MOS_PAGE_SIZE;
                new->paddr = end_addr;
                new->n_pages = part_2_size / MOS_PAGE_SIZE;
            }
            freed = true;
            break;
        }
        prev = this;
        this = next;
    }

    if (!freed)
        mos_panic(PTR_FMT "-" PTR_FMT " is not in the freelist.", start_addr, end_addr);

    if (needs_cleanup)
    {
        // firstly find who is at 'pmem_freelist_count' that should be copied to 'cleanup_target_memptr'
        // minus 1 because it's a count, make it an index
        pmem_range_t *copy_source = &pmem_freelist_storage[pmem_freelist_count - 1];
        pmem_freelist_count--;

        pmem_range_t *r_prev = NULL;
        for (pmem_range_t *r_this = pmem_freelist; r_this != copy_source && r_this->next; r_this = r_this->next)
            r_prev = r_this;

        if (cleanup_target_memptr != copy_source)
        {
            memmove(cleanup_target_memptr, copy_source, sizeof(pmem_range_t));
            memset(copy_source, 0, sizeof(pmem_range_t)); // prevent from being used again

            if (copy_source != r_prev->next)
            {
                pmem_freelist = cleanup_target_memptr;
            }
            else
            {
                r_prev->next = cleanup_target_memptr;
            }
        }
    }
}

#undef this_start
#undef this_end

uintptr_t pmem_freelist_find_free(size_t pages)
{
    pmem_range_t *this = pmem_freelist;
    while (this)
    {
        if (this->n_pages >= pages)
        {
            // ! do not remove the range from the freelist
            uintptr_t addr = this->paddr;
            mos_debug("allocated %zu pages from freelist, starting at " PTR_FMT, pages, addr);
            return addr;
        }
        this = this->next;
    }

    // !! OOM
    mos_warn("out of memory?");
    MOS_UNREACHABLE();
    return 0;
}
