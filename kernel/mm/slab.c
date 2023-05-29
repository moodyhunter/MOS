// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-License-Identifier: BSD-2-Clause

#include "mos/mm/slab.h"

#include "mos/mm/paging/paging.h"
#include "mos/platform/platform.h"

#include <mos/kconfig.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mos_global.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    spinlock_t lock;
    void **first_free;
    size_t ent_size;
} slab_t;

typedef struct
{
    slab_t *slab;
} slab_header_t;

typedef struct
{
    size_t pages;
    size_t size;
} slab_metadata_t;

static const size_t slab_sizes[] = { 8, 16, 24, 32, 48, 64, 128, 256, 512, 1024 };
static slab_t slabs[10];

static inline slab_t *slab_for(size_t size)
{
    for (size_t i = 0; i < MOS_ARRAY_SIZE(slabs); i++)
    {
        slab_t *slab = &slabs[i];
        if (slab->ent_size >= size)
            return slab;
    }
    return NULL;
}

ptr_t get_free_pages(size_t n)
{
    vmblock_t block = mm_alloc_pages(platform_info->kernel_pgd, n, MOS_ADDR_KERNEL_HEAP, VALLOC_DEFAULT, VM_RW);
    return block.vaddr;
}

void free_page(ptr_t page, size_t n)
{
    mm_unmap_pages(platform_info->kernel_pgd, page, n);
}

static void create_slab(slab_t *slab, size_t ent_size)
{
    slab->lock = (spinlock_t) SPINLOCK_INIT;
    slab->first_free = (void **) get_free_pages(1);
    slab->ent_size = ent_size;

    const size_t header_offset = ALIGN_UP(sizeof(slab_header_t), ent_size);
    const size_t available_size = MOS_PAGE_SIZE - header_offset;

    slab_header_t *const slab_ptr = (slab_header_t *) slab->first_free;
    slab_ptr->slab = slab;
    slab->first_free = (void **) ((ptr_t) slab->first_free + header_offset);

    void **arr = (void **) slab->first_free;
    size_t max = available_size / ent_size - 1;
    size_t fact = ent_size / sizeof(void *);

    for (size_t i = 0; i < max; i++)
    {
        arr[i * fact] = &arr[(i + 1) * fact];
    }
    arr[max * fact] = NULL;
}

static void *alloc_from_slab(slab_t *slab)
{
    spinlock_acquire(&slab->lock);

    if (slab->first_free == NULL)
    {
        create_slab(slab, slab->ent_size);
    }

    void **old_free = slab->first_free;
    slab->first_free = *old_free;
    memset(old_free, 0, slab->ent_size);

    spinlock_release(&slab->lock);
    return old_free;
}

static void free_in_slab(slab_t *slab, ptr_t addr)
{
    if (!addr)
        return;

    spinlock_acquire(&slab->lock);

    void **new_head = (void **) addr;
    *new_head = slab->first_free;
    slab->first_free = new_head;

    spinlock_release(&slab->lock);
}

void slab_init(void)
{
    for (size_t i = 0; i < MOS_ARRAY_SIZE(slab_sizes); i++)
        create_slab(&slabs[i], slab_sizes[i]);
}

void *slab_alloc(size_t size)
{
    slab_t *const slab = slab_for(size);
    if (slab != NULL)
    {
        return alloc_from_slab(slab);
    }

    const size_t page_count = ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE;
    ptr_t ret = get_free_pages(page_count + 1); // pmm_alloc(page_count + 1);
    if (!ret)
        return NULL;

    slab_metadata_t *metadata = (slab_metadata_t *) ret;
    metadata->pages = page_count;
    metadata->size = size;

    return (void *) ((ptr_t) ret + MOS_PAGE_SIZE);
}

void *slab_calloc(size_t nmemb, size_t size)
{
    void *ptr = slab_alloc(nmemb * size);
    if (!ptr)
        return NULL;

    memset(ptr, 0, nmemb * size);
    return ptr;
}

void *slab_realloc(void *oldptr, size_t new_size)
{
    if (!oldptr)
        return slab_alloc(new_size);

    const ptr_t addr = (ptr_t) oldptr;
    if (is_aligned(addr, MOS_PAGE_SIZE))
    {
        slab_metadata_t *metadata = (slab_metadata_t *) (addr - MOS_PAGE_SIZE);
        if (ALIGN_UP_TO_PAGE(metadata->size) == ALIGN_UP_TO_PAGE(new_size))
        {
            metadata->size = new_size;
            return oldptr;
        }

        void *new_addr = slab_alloc(new_size);
        if (!new_addr)
            return NULL;

        memcpy(new_addr, oldptr, MIN(metadata->size, new_size));

        slab_free(oldptr);
        return new_addr;
    }

    slab_header_t *slab_header = (slab_header_t *) ALIGN_DOWN_TO_PAGE(addr);
    slab_t *slab = slab_header->slab;

    if (new_size > slab->ent_size)
    {
        void *new_addr = slab_alloc(new_size);
        if (new_addr == NULL)
        {
            return NULL;
        }

        memcpy(new_addr, oldptr, slab->ent_size);
        free_in_slab(slab, addr);
        return new_addr;
    }

    return oldptr;
}

void slab_free(const void *ptr)
{
    if (!ptr)
        return;

    const ptr_t addr = (ptr_t) ptr;
    if (is_aligned(addr, MOS_PAGE_SIZE))
    {
        slab_metadata_t *metadata = (slab_metadata_t *) (addr - MOS_PAGE_SIZE);
        free_page((ptr_t) metadata, metadata->pages + 1);
        return;
    }

    const slab_header_t *header = (slab_header_t *) ALIGN_DOWN_TO_PAGE(addr);
    free_in_slab(header->slab, addr);
}
