// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmalloc.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#define RESERVED_LOMEM                   1 MB
#define PMEM_FREELIST_SIZE_FOR(mem_size) (((mem_size) / 2 / MOS_PAGE_SIZE) * sizeof(pmlist_node_t))

typedef struct _pmlist_node_t
{
    struct _pmlist_node_t *next;
    uintptr_t paddr;
    size_t n_pages;
} pmlist_node_t;

// Do not mark as const, or the compiler will put it in .rodata
static pmlist_node_t pmlist_storage[PMEM_FREELIST_SIZE_FOR(MOS_MAX_VADDR)] __aligned(MOS_PAGE_SIZE) = { 0 };
static pmlist_node_t *pmlist_head = (pmlist_node_t *) pmlist_storage;
static size_t pmlist_count = 0;
static spinlock_t pmlist_lock = SPINLOCK_INIT;

static size_t freelist_size()
{
    return ALIGN_UP_TO_PAGE(PMEM_FREELIST_SIZE_FOR(platform_info->mem.available));
}

void mos_pmm_setup(void)
{
    pr_info("Initializing Physical Memory Allocator");
    const size_t list_size = freelist_size();
    pr_info2("%zu bytes (aligned) required for physical memory freelist", list_size);

    memzero(pmlist_head, list_size);

    // add current physical memory region to the freelist
    for (size_t i = 0; i < platform_info->mem_regions.count; i++)
    {
        memregion_t *r = &platform_info->mem_regions.regions[i];
        if (!r->available)
            continue;

        if (r->address + r->size_bytes < RESERVED_LOMEM)
        {
            pr_emph("ignored low memory: " PTR_FMT "-" PTR_FMT " (%zu bytes)", r->address, r->address + r->size_bytes, r->size_bytes);
            continue;
        }

        size_t alignment_loss = pmalloc_release_region(r->address, r->size_bytes);
        if (alignment_loss)
            pr_emph("%zu bytes of memory loss due to alignment", alignment_loss);
    }

    pr_info("Physical Memory Allocator initialized");
#if MOS_DEBUG_FEATURE(pmm)
    pmalloc_dump();
    declare_panic_hook(pmalloc_dump);
    install_panic_hook(&pmalloc_dump_holder);
#endif
}

void pmalloc_dump()
{
    pr_info("Physical Memory Allocator dump:");
    pmlist_node_t *this = pmlist_head;
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

uintptr_t pmalloc_alloc(size_t pages)
{
    spinlock_acquire(&pmlist_lock);
    pmlist_node_t *this = pmlist_head;
    while (this)
    {
        if (this->n_pages >= pages)
        {
            const uintptr_t addr = this->paddr; // ! pmalloc_acquire_region changes `this->paddr`, make a copy first
            mos_debug(pmm, "allocated %zu pages from freelist, starting at " PTR_FMT, pages, addr);

            if (this->n_pages == pages)
            {
                // remove this node from the list
                if (this == pmlist_head)
                    pmlist_head = this->next;
                else
                {
                    pmlist_node_t *prev = pmlist_head;
                    while (prev->next != this)
                        prev = prev->next;
                    prev->next = this->next;
                }
                pmlist_count--;
            }
            else
            {
                // shrink this node
                this->paddr += pages * MOS_PAGE_SIZE;
                this->n_pages -= pages;
            }

            spinlock_release(&pmlist_lock);
            return addr;
        }
        this = this->next;
    }

    mos_panic("no free physical memory found in freelist, %zu pages requested", pages);
}

#define this_start (this->paddr)
#define this_end   (this_start + this->n_pages * MOS_PAGE_SIZE)

void pmalloc_acquire_region(uintptr_t start_addr, size_t size_bytes)
{
    const uintptr_t end_addr = start_addr + size_bytes;

    // the addresses must be aligned to page size
    MOS_ASSERT(start_addr % MOS_PAGE_SIZE == 0);
    MOS_ASSERT(end_addr % MOS_PAGE_SIZE == 0);

    const size_t pages_in_region = (end_addr - start_addr) / MOS_PAGE_SIZE;

    mos_debug(pmm, "removing physical memory region " PTR_FMT "-" PTR_FMT " from freelist.", start_addr, end_addr);

    bool needs_cleanup = false;
    bool freed = false;
    void *cleanup_target_memptr = NULL;

    spinlock_acquire(&pmlist_lock);

    pmlist_node_t *this = pmlist_head;
    pmlist_node_t *prev = NULL;
    while (this)
    {
        pmlist_node_t *next = this->next;
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
                mos_debug(pmm, "pmem block now starts at " PTR_FMT ", with %zu pages", this_start, this->n_pages);
            }
            else if (part_1_size != 0 && part_2_size == 0)
            {
                // part 2 is empty, which means we are removing the tail of part 1
                mos_debug(pmm, "shrunk " PTR_FMT "-" PTR_FMT ": ends at " PTR_FMT, this_start, this_end, this_start + (this->n_pages - pages_in_region) * MOS_PAGE_SIZE);
                this->n_pages -= pages_in_region;
            }
            else if (part_1_size == 0 && part_2_size == 0)
            {
                // part 1 and part 2 are empty, so remove this entry
                if (prev)
                    prev->next = next;
                else
                    pmlist_head = next;

                // have to do some cleanup
                needs_cleanup = true;
                cleanup_target_memptr = this;
                mos_debug(pmm, "removed " PTR_FMT "-" PTR_FMT " from freelist.", this_start, this_end);
            }
            else
            {
                // neither part 1 nor part 2 is empty, so we have to allocate a new entry for part 2
                mos_debug(pmm, "split " PTR_FMT "-" PTR_FMT " into " PTR_FMT "-" PTR_FMT " and " PTR_FMT "-" PTR_FMT, this_start, this_end, this_start, start_addr,
                          end_addr, end_addr + part_2_size);
                pmlist_node_t *new = (pmlist_node_t *) pmlist_storage + pmlist_count++;
                memset(new, 0, sizeof(pmlist_node_t));
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
        // firstly find who is at 'pmlist_count' that should be copied to 'cleanup_target_memptr'
        // minus 1 because it's a count, make it an index
        pmlist_node_t *copy_source = &pmlist_storage[pmlist_count - 1];
        pmlist_count--;

        pmlist_node_t *r_prev = pmlist_head;
        for (pmlist_node_t *r_this = r_prev; r_this != copy_source && r_this->next; r_this = r_this->next)
            r_prev = r_this;

        if (cleanup_target_memptr != copy_source)
        {
            memmove(cleanup_target_memptr, copy_source, sizeof(pmlist_node_t));
            memzero(copy_source, sizeof(pmlist_node_t)); // prevent from being used again

            if (copy_source != r_prev->next)
            {
                pmlist_head = cleanup_target_memptr;
            }
            else
            {
                r_prev->next = cleanup_target_memptr;
            }
        }
    }

    spinlock_release(&pmlist_lock);
}

size_t pmalloc_release_region(uintptr_t start_addr, size_t size_bytes)
{
    const uintptr_t aligned_start = ALIGN_UP_TO_PAGE(start_addr);
    const uintptr_t aligned_end = ALIGN_DOWN_TO_PAGE(start_addr + size_bytes);

    const size_t pages_in_region = (aligned_end - aligned_start) / MOS_PAGE_SIZE;

    mos_debug(pmm, "adding physical memory region " PTR_FMT "-" PTR_FMT " to freelist.", aligned_start, aligned_end);

    spinlock_acquire(&pmlist_lock);

    pmlist_node_t *this = pmlist_head;
    pmlist_node_t *prev = NULL;

    while (this)
    {
        // * if the aligned start/end address is within the current region, it overlaps
        if ((this_start <= aligned_start && aligned_start < this_end) || (this_start < aligned_end && aligned_end <= this_end))
        {
            mos_panic("new pmem " PTR_FMT "-" PTR_FMT " overlaps with " PTR_FMT "-" PTR_FMT, aligned_start, aligned_end, this_start, this_end);
        }

        // prepend to 'this' region
        if (aligned_end == this_start)
        {
            uintptr_t new_end = this_start + pages_in_region * MOS_PAGE_SIZE;
            mos_debug(pmm, "enlarge range [" PTR_FMT "-" PTR_FMT "]: starts at " PTR_FMT, this_start, this_end, new_end);
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
                mos_debug(pmm, "enlarged " PTR_FMT "-" PTR_FMT ": ends at " PTR_FMT, prev_start, prev_end, new_ends);
                goto end;
            }
        }

        // same as above, we only 'prepend' the region before 'this'
        if (aligned_end < this_start)
        {
            pmlist_node_t *new_range = &pmlist_storage[pmlist_count++];
            new_range->paddr = aligned_start;
            new_range->n_pages = pages_in_region;
            new_range->next = this;
            if (prev)
                prev->next = new_range;
            else
                pmlist_head = new_range;
            goto end;
        }

        prev = this;
        this = this->next;
    }

    MOS_ASSERT(this == NULL && prev != NULL);
    pmlist_node_t *new_range = &pmlist_storage[pmlist_count++];
    prev->next = new_range;

    // the freelist is in the correct order, so we can insert the new range here
    new_range->paddr = aligned_start;
    new_range->n_pages = pages_in_region;
    new_range->next = NULL;

end:
    spinlock_release(&pmlist_lock);
    // return the amount of memory that was lost due to alignment
    return (aligned_start - start_addr) + ((start_addr + size_bytes) - aligned_end);
}

#undef this_start
#undef this_end
