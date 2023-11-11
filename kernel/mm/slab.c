// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-License-Identifier: BSD-2-Clause

#include "mos/mm/slab.h"

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/mm.h"
#include "mos/mm/paging/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/setup.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mos_global.h>
#include <mos_stdlib.h>
#include <mos_string.h>

typedef struct
{
    slab_t *slab;
} slab_header_t;

typedef struct
{
    size_t pages;
    size_t size;
} slab_metadata_t;

static const struct
{
    size_t size;
    const char *name;
} BUILTIN_SLAB_SIZES[] = {
    { 4, "builtin-4" },       { 8, "builtin-8" },     { 16, "builtin-16" },   { 24, "builtin-24" },   //
    { 32, "builtin-32" },     { 48, "builtin-48" },   { 64, "builtin-64" },   { 96, "builtin-96" },   //
    { 128, "builtin-128" },   { 256, "builtin-256" }, { 384, "builtin-384" }, { 512, "builtin-512" }, //
    { 1024, "builtin-1024" },
    // larger slab sizes are not required
    // they can be allocated directly by allocating pages
};

static slab_t slab_slab = { 0 };

static slab_t slabs[MOS_ARRAY_SIZE(BUILTIN_SLAB_SIZES)] = { 0 };
static list_head slabs_list = LIST_HEAD_INIT(slabs_list);

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

static ptr_t slab_impl_new_page(size_t n)
{
    phyframe_t *pages = mm_get_free_pages(n);
    mmstat_inc(MEM_SLAB, n);
    return phyframe_va(pages);
}

static void slab_impl_free_page(ptr_t page, size_t n)
{
    mmstat_dec(MEM_SLAB, n);
    mm_free_pages(va_phyframe(page), n);
}

static void slab_init_one(slab_t *slab, const char *name, size_t size)
{
    MOS_ASSERT_X(size < MOS_PAGE_SIZE, "current slab implementation does not support slabs larger than a page");
    pr_dinfo2(slab, "slab: registering slab for '%s' with %zu bytes", name, size);
    linked_list_init(list_node(slab));
    list_node_append(&slabs_list, list_node(slab));
    slab->lock = (spinlock_t) SPINLOCK_INIT;
    slab->first_free = 0;
    slab->nobjs = 0;
    slab->name = name;
    slab->ent_size = size;
}

static void slab_allocate_mem(slab_t *slab)
{
    pr_dinfo2(slab, "renew slab for '%s' with %zu bytes", slab->name, slab->ent_size);
    slab->first_free = slab_impl_new_page(1);
    if (unlikely(!slab->first_free))
    {
        mos_panic("slab: failed to allocate memory for slab");
        return;
    }

    const size_t header_offset = ALIGN_UP(sizeof(slab_header_t), slab->ent_size);
    const size_t available_size = MOS_PAGE_SIZE - header_offset;

    slab_header_t *const slab_ptr = (slab_header_t *) slab->first_free;
    slab_ptr->slab = slab;
    pr_dinfo2(slab, "slab header is at %p", (void *) slab_ptr);
    slab->first_free = (ptr_t) slab->first_free + header_offset;

    void **arr = (void **) slab->first_free;
    const size_t max_n = available_size / slab->ent_size - 1;
    const size_t fact = slab->ent_size / sizeof(void *);

    for (size_t i = 0; i < max_n; i++)
    {
        arr[i * fact] = &arr[(i + 1) * fact];
    }
    arr[max_n * fact] = NULL;
}

static void slab_init(void)
{
    pr_dinfo2(slab, "initializing the slab allocator");

    slab_init_one(&slab_slab, "slab_t", sizeof(slab_t));
    slab_allocate_mem(&slab_slab);

    for (size_t i = 0; i < MOS_ARRAY_SIZE(BUILTIN_SLAB_SIZES); i++)
    {
        slab_init_one(&slabs[i], BUILTIN_SLAB_SIZES[i].name, BUILTIN_SLAB_SIZES[i].size);
        slab_allocate_mem(&slabs[i]);
    }
}

MOS_INIT(POST_MM, slab_init);

static void kmemcache_free(slab_t *slab, const void *addr);

void *slab_alloc(size_t size)
{
    slab_t *const slab = slab_for(size);
    if (likely(slab))
        return kmemcache_alloc(slab);

    const size_t page_count = ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE;
    const ptr_t ret = slab_impl_new_page(page_count + 1);
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
        if (!new_addr)
            return NULL;

        memcpy(new_addr, oldptr, slab->ent_size);
        kmemcache_free(slab, oldptr);
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
        slab_impl_free_page((ptr_t) metadata, metadata->pages + 1);
        return;
    }

    const slab_header_t *header = (slab_header_t *) ALIGN_DOWN_TO_PAGE(addr);
    kmemcache_free(header->slab, ptr);
}

// ======================

slab_t *kmemcache_create(const char *name, size_t ent_size)
{
    slab_t *slab = kmemcache_alloc(&slab_slab);
    slab_init_one(slab, name, ent_size);
    slab_allocate_mem(slab);
    return slab;
}

void *kmemcache_alloc(slab_t *slab)
{
    pr_dinfo2(slab, "allocating from slab '%s'", slab->name);
    spinlock_acquire(&slab->lock);

    if (slab->first_free == 0)
    {
        // renew a slab
        slab_allocate_mem(slab);
    }

    ptr_t *alloc = (ptr_t *) slab->first_free;
    slab->first_free = *alloc; // next free entry
    memset(alloc, 0, slab->ent_size);

#if MOS_DEBUG_FEATURE(slab)
    pr_cont(" -> %p", (void *) alloc);
#endif

    slab->nobjs++;
    spinlock_release(&slab->lock);
    return alloc;
}

static void kmemcache_free(slab_t *slab, const void *addr)
{
    pr_dinfo2(slab, "freeing from slab '%s'", slab->name);
    if (!addr)
        return;

    spinlock_acquire(&slab->lock);

    ptr_t *new_head = (ptr_t *) addr;
    *new_head = slab->first_free;
    slab->first_free = (ptr_t) new_head;
    slab->nobjs--;

    spinlock_release(&slab->lock);
}

// ! sysfs support

static bool slab_sysfs_status(sysfs_file_t *f)
{
    list_foreach(slab_t, slab, slabs_list)
    {
        sysfs_printf(f, "%15s, ent_size=%5zu, first_free=" PTR_FMT ", %5zu objects\n", slab->name, slab->ent_size, slab->first_free, slab->nobjs);
    }

    return true;
}

static sysfs_item_t slab_sysfs_items[] = {
    SYSFS_RO_ITEM("status", slab_sysfs_status),
};

SYSFS_AUTOREGISTER(slab, slab_sysfs_items);
