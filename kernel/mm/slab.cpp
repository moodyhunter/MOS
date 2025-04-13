// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/slab.hpp"

#include "mos/assert.hpp"
#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/misc/setup.hpp"
#include "mos/mm/mm.hpp"

#include <algorithm>
#include <mos/allocator.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/mos_global.h>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

struct slab_header_t
{
    slab_t *slab;
};

struct slab_metadata_t
{
    size_t pages;
    size_t size;
};

// larger slab sizes are not required, they can be allocated directly by allocating pages
static const struct
{
    size_t size;
    const char *name;
} BUILTIN_SLAB_SIZES[] = {
    { 4, "builtin-4" },       { 8, "builtin-8" },     { 16, "builtin-16" },   { 24, "builtin-24" },   //
    { 32, "builtin-32" },     { 48, "builtin-48" },   { 64, "builtin-64" },   { 96, "builtin-96" },   //
    { 128, "builtin-128" },   { 256, "builtin-256" }, { 384, "builtin-384" }, { 512, "builtin-512" }, //
    { 1024, "builtin-1024" },
};

static slab_t slabs[MOS_ARRAY_SIZE(BUILTIN_SLAB_SIZES)];
static list_head slabs_list;

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
    if (pages == nullptr)
        return 0;
    mmstat_inc(MEM_SLAB, n);
    return phyframe_va(pages);
}

static void slab_impl_free_page(ptr_t page, size_t n)
{
    mmstat_dec(MEM_SLAB, n);
    mm_free_pages(va_phyframe(page), n);
}

static void slab_allocate_mem(slab_t *s)
{
    dInfo2<slab> << "renew slab for '" << s->name << "' with " << s->ent_size << " bytes";
    s->first_free = slab_impl_new_page(1);
    if (unlikely(!s->first_free))
    {
        mos_panic("slab: failed to allocate memory for slab");
        return;
    }

    const size_t header_offset = ALIGN_UP(sizeof(slab_header_t), s->ent_size);
    const size_t available_size = MOS_PAGE_SIZE - header_offset;

    slab_header_t *const slab_ptr = (slab_header_t *) s->first_free;
    slab_ptr->slab = s;
    dInfo2<slab> << "slab header is at " << (void *) slab_ptr;
    s->first_free = (ptr_t) s->first_free + header_offset;

    void **arr = (void **) s->first_free;
    const size_t max_n = available_size / s->ent_size - 1;
    const size_t fact = s->ent_size / sizeof(void *);

    for (size_t i = 0; i < max_n; i++)
    {
        arr[i * fact] = &arr[(i + 1) * fact];
    }
    arr[max_n * fact] = NULL;
}

static void slab_init_one(slab_t *slab, const char *name, size_t size)
{
    MOS_ASSERT_X(size < MOS_PAGE_SIZE, "current slab implementation does not support slabs larger than a page, %zu bytes requested", size);
    linked_list_init(list_node(slab));
    list_node_append(&slabs_list, list_node(slab));
    slab->lock = SPINLOCK_INIT;
    slab->first_free = 0;
    slab->nobjs = 0;
    slab->name = name;
    slab->type_name = "<unsure>";
    slab->ent_size = size;
}

void slab_init(void)
{
    dInfo2<slab> << "initializing the slab allocator";
    for (size_t i = 0; i < MOS_ARRAY_SIZE(BUILTIN_SLAB_SIZES); i++)
        slab_init_one(&slabs[i], BUILTIN_SLAB_SIZES[i].name, BUILTIN_SLAB_SIZES[i].size);
}

void slab_register(slab_t *s)
{
    dInfo2<slab> << "slab: registering slab for '" << s->name << "' with " << s->ent_size << " bytes";
    linked_list_init(list_node(s));
    list_node_append(&slabs_list, list_node(s));
}

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

        memcpy(new_addr, oldptr, std::min(metadata->size, new_size));

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
    dInfo2<slab> << "freeing memory at " << ptr;
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

void *kmemcache_alloc(slab_t *s)
{
    MOS_ASSERT_X(s->ent_size > 0, "slab: invalid slab entry size %zu", s->ent_size);
    dInfo2<slab> << "allocating from slab '" << s->name << "'";
    spinlock_acquire(&s->lock);

    if (s->first_free == 0)
    {
        // renew a slab
        slab_allocate_mem(s);
    }

    ptr_t *alloc = (ptr_t *) s->first_free;
    dCont<slab> << " -> " << (void *) alloc;

    // sanitize the memory
    MOS_ASSERT_X((ptr_t) alloc >= MOS_KERNEL_START_VADDR, "slab: invalid memory address %p", (void *) alloc);

    s->first_free = *alloc; // next free entry
    memset(alloc, 0, s->ent_size);

    s->nobjs++;
    spinlock_release(&s->lock);
    return alloc;
}

void kmemcache_free(slab_t *s, const void *addr)
{
    dInfo2<slab> << "freeing from slab '" << s->name << "'";
    if (!addr)
        return;

    spinlock_acquire(&s->lock);

    ptr_t *new_head = (ptr_t *) addr;
    *new_head = s->first_free;
    s->first_free = (ptr_t) new_head;
    s->nobjs--;

    spinlock_release(&s->lock);
}

// ! sysfs support

static bool slab_sysfs_slabinfo(sysfs_file_t *f)
{
    sysfs_printf(f, "%20s \t%-10s %-18s \t%-8s    %s\n\n", "", "Size", "First Free", "Objects", "Type Name");
    list_foreach(slab_t, slab, slabs_list)
    {
        sysfs_printf(f, "%20s:\t%-10zu " PTR_FMT " \t%-8zu    %.*s\n", //
                     slab->name.data(),                                //
                     slab->ent_size,                                   //
                     slab->first_free,                                 //
                     slab->nobjs,                                      //
                     (int) slab->type_name.size(),                     //
                     slab->type_name.data()                            //
        );
    }

    return true;
}

MOS_INIT(SYSFS, slab_sysfs_init)
{
    static sysfs_item_t slabinfo = SYSFS_RO_ITEM("slabinfo", slab_sysfs_slabinfo);
    sysfs_register_root_file(&slabinfo);
}
