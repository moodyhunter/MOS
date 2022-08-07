// SPDX-Licence-Identifier: BSD-3-Clause
// Durand's Ridiculously Amazing Super Duper Memory functions.

#include "mos/mm/liballoc.h"

#include "lib/string.h"
#include "mos/mm/paging.h"
#include "mos/printk.h"

#if MOS_MM_LIBALLOC_DEBUG
unsigned int l_allocated = 0; //< The real amount of memory allocated.
unsigned int l_inuse = 0;     //< The amount of memory in use (malloc'ed).
#endif

#define LIBALLOC_MAGIC 0x11223344
#define MAXCOMPLETE    5
#define MAXEXP         32
#define MINEXP         8

#define MODE_BEST    0
#define MODE_INSTANT 1

#define LIBALLOC_MODE MODE_BEST

static int l_pageSize = 0;
static bool l_initialized = 0;

static struct boundary_tag *l_freePages[MAXEXP]; // Allowing for 2^MAXEXP blocks
static int l_completePages[MAXEXP];              // Allowing for 2^MAXEXP blocks
static const u32 l_pageCount = 16;               // Minimum number of pages to allocate.

void *(*liballoc_page_alloc)(size_t n) = mm_page_alloc;
bool (*liballoc_page_free)(void *ptr, size_t n) = mm_page_free;

/** Returns the exponent required to manage 'size' amount of memory.
 *
 *  Returns n where  2^n <= size < 2^(n+1)
 */
static inline int getexp(unsigned int size)
{
    if (size < (1 << MINEXP))
        return -1; // Smaller than the quantum.

    int shift = MINEXP;
    while (shift < MAXEXP)
    {
        if ((1u << shift) > size)
            break;
        shift += 1;
    }
    return shift - 1;
}

#if MOS_MM_LIBALLOC_DEBUG
static void dump_array()
{
    int i = 0;
    struct boundary_tag *tag = NULL;

    mos_debug("------ Free pages array ---------");
    mos_debug("System memory allocated: %i", l_allocated);
    mos_debug("Memory in used (malloc'ed): %i", l_inuse);

    for (i = 0; i < MAXEXP; i++)
    {
        printk("%.2i(%i): ", i, l_completePages[i]);

        tag = l_freePages[i];
        while (tag != NULL)
        {
            if (tag->split_left != NULL)
                printk("*");
            printk("%zu", tag->real_size);
            if (tag->split_right != NULL)
                printk("*");

            printk(" ");
            tag = tag->next;
        }
        printk("\n");
    }

    mos_debug("'*' denotes a split to the left/right of a tag");
}
#endif

static inline void insert_tag(struct boundary_tag *tag, int index)
{
    int realIndex;
    if (index < 0)
    {
        realIndex = getexp(tag->real_size - sizeof(struct boundary_tag));
        if (realIndex < MINEXP)
            realIndex = MINEXP;
    }
    else
        realIndex = index;

    tag->index = realIndex;

    if (l_freePages[realIndex] != NULL)
    {
        l_freePages[realIndex]->prev = tag;
        tag->next = l_freePages[realIndex];
    }

    l_freePages[realIndex] = tag;
}

static inline void remove_tag(struct boundary_tag *tag)
{
    if (l_freePages[tag->index] == tag)
        l_freePages[tag->index] = tag->next;

    if (tag->prev != NULL)
        tag->prev->next = tag->next;
    if (tag->next != NULL)
        tag->next->prev = tag->prev;

    tag->next = NULL;
    tag->prev = NULL;
    tag->index = -1;
}

static inline struct boundary_tag *melt_left(struct boundary_tag *tag)
{
    struct boundary_tag *left = tag->split_left;

    left->real_size += tag->real_size;
    left->split_right = tag->split_right;

    if (tag->split_right != NULL)
        tag->split_right->split_left = left;

    return left;
}

static inline struct boundary_tag *absorb_right(struct boundary_tag *tag)
{
    struct boundary_tag *right = tag->split_right;

    remove_tag(right); // Remove right from free pages.

    tag->real_size += right->real_size;

    tag->split_right = right->split_right;
    if (right->split_right != NULL)
        right->split_right->split_left = tag;

    return tag;
}

static inline struct boundary_tag *split_tag(struct boundary_tag *tag)
{
    unsigned int remainder = tag->real_size - sizeof(struct boundary_tag) - tag->size;

    struct boundary_tag *new_tag = (struct boundary_tag *) ((unsigned int) tag + sizeof(struct boundary_tag) + tag->size);

    new_tag->magic = LIBALLOC_MAGIC;
    new_tag->real_size = remainder;

    new_tag->next = NULL;
    new_tag->prev = NULL;

    new_tag->split_left = tag;
    new_tag->split_right = tag->split_right;

    if (new_tag->split_right != NULL)
        new_tag->split_right->split_left = new_tag;
    tag->split_right = new_tag;

    tag->real_size -= new_tag->real_size;

    insert_tag(new_tag, -1);

    return new_tag;
}

static struct boundary_tag *allocate_new_tag(unsigned int size)
{
    // This is how much space is required.
    unsigned int usage = size + sizeof(struct boundary_tag);

    // Perfect amount of space
    unsigned int pages = usage / l_pageSize;
    if ((usage % l_pageSize) != 0)
        pages += 1;

    // Make sure it's >= the minimum size.
    if (pages < l_pageCount)
        pages = l_pageCount;

    struct boundary_tag *tag = (struct boundary_tag *) liballoc_page_alloc(pages);

    if (tag == NULL)
        return NULL; // uh oh, we ran out of memory.

    tag->magic = LIBALLOC_MAGIC;
    tag->size = size;
    tag->real_size = pages * l_pageSize;
    tag->index = -1;

    tag->next = NULL;
    tag->prev = NULL;
    tag->split_left = NULL;
    tag->split_right = NULL;

#if MOS_MM_LIBALLOC_DEBUG
    mos_debug("Resource allocated %p of %i pages (%i bytes) for %i size.", (void *) tag, pages, pages * l_pageSize, size);
    l_allocated += pages * l_pageSize;
    mos_debug("Total memory usage = %i KB", (int) ((l_allocated / (1024))));
#endif

    return tag;
}

void liballoc_init(int page_size)
{
    MOS_ASSERT(!l_initialized);
    l_pageSize = page_size;
#if MOS_MM_LIBALLOC_DEBUG
    mos_debug("liballoc initializing.");
#endif
    for (int index = 0; index < MAXEXP; index++)
    {
        l_freePages[index] = NULL;
        l_completePages[index] = 0;
    }
    l_initialized = true;
}

void *liballoc_malloc(size_t size)
{
    MOS_ASSERT(l_initialized);
    int index = getexp(size) + LIBALLOC_MODE;
    if (index < MINEXP)
        index = MINEXP;

#if MOS_MM_LIBALLOC_LOCKS
    liballoc_lock();
#endif

    // Find one big enough, start at the front of the list.
    struct boundary_tag *tag = l_freePages[index];
    while (tag != NULL)
    {
        // If there's enough space in this tag.
        if ((tag->real_size - sizeof(struct boundary_tag)) >= (size + sizeof(struct boundary_tag)))
        {
#if MOS_MM_LIBALLOC_DEBUG
            mos_debug("Tag search found %zu >= %zu", (tag->real_size - sizeof(struct boundary_tag)), (size + sizeof(struct boundary_tag)));
#endif
            break;
        }
        tag = tag->next;
    }

    // No page found. Make one.
    if (tag == NULL)
    {
        if ((tag = allocate_new_tag(size)) == NULL)
        {
#if MOS_MM_LIBALLOC_LOCKS
            liballoc_unlock();
#endif
            return NULL;
        }

        index = getexp(tag->real_size - sizeof(struct boundary_tag));
    }
    else
    {
        remove_tag(tag);

        if ((tag->split_left == NULL) && (tag->split_right == NULL))
            l_completePages[index] -= 1;
    }

    // We have a free page.  Remove it from the free pages list.

    tag->size = size;

    // Removed... see if we can re-use the excess space.

#if MOS_MM_LIBALLOC_DEBUG
    mos_debug("Found tag with %zu bytes available (requested %zu bytes, leaving %zu), which has exponent: %i (%i bytes)",
              tag->real_size - sizeof(struct boundary_tag), size, tag->real_size - size - sizeof(struct boundary_tag), index, 1 << index);
#endif

    unsigned int remainder = tag->real_size - size - sizeof(struct boundary_tag) * 2; // Support a new tag + remainder

    if (((int) (remainder) > 0) /*&& ( (tag->real_size - remainder) >= (1<<MINEXP))*/)
    {
        int childIndex = getexp(remainder);

        if (childIndex >= 0)
        {
#if MOS_MM_LIBALLOC_DEBUG
            mos_debug("Seems to be splittable: %i >= 2^%i .. %i", remainder, childIndex, (1 << childIndex));
#endif
            struct boundary_tag *new_tag = split_tag(tag);
            MOS_UNUSED(new_tag);
#if MOS_MM_LIBALLOC_DEBUG
            mos_debug("Old tag has become %zu bytes, new tag is now %zu bytes (%i exp)", tag->real_size, new_tag->real_size, new_tag->index);
#endif
        }
    }

    void *ptr = (void *) ((unsigned int) tag + sizeof(struct boundary_tag));

#if MOS_MM_LIBALLOC_DEBUG
    l_inuse += size;
    mos_debug("malloc: %p,  %i, %i", ptr, (int) l_inuse / 1024, (int) l_allocated / 1024);
    dump_array();
#endif
#if MOS_MM_LIBALLOC_LOCKS
    liballoc_unlock();
#endif
    return ptr;
}

void liballoc_free(void *ptr)
{
    MOS_ASSERT(l_initialized);
    if (ptr == NULL)
        return;

#if MOS_MM_LIBALLOC_LOCKS
    liballoc_lock();
#endif

    struct boundary_tag *tag = (struct boundary_tag *) ((unsigned int) ptr - sizeof(struct boundary_tag));

    if (tag->magic != LIBALLOC_MAGIC)
    {
#if MOS_MM_LIBALLOC_LOCKS
        liballoc_unlock(); // release the lock
#endif
        return;
    }

#if MOS_MM_LIBALLOC_DEBUG
    l_inuse -= tag->size;
    mos_debug("free: %p, %i, %i", ptr, (int) l_inuse / 1024, (int) l_allocated / 1024);
#endif

    // MELT LEFT...
    while ((tag->split_left != NULL) && (tag->split_left->index >= 0))
    {
#if MOS_MM_LIBALLOC_DEBUG
        mos_debug("Melting tag left into available memory. Left was %zu, becomes %zu (%zu)", tag->split_left->real_size,
                  tag->split_left->real_size + tag->real_size, tag->split_left->real_size);
#endif
        tag = melt_left(tag);
        remove_tag(tag);
    }

    // MELT RIGHT...
    while ((tag->split_right != NULL) && (tag->split_right->index >= 0))
    {
#if MOS_MM_LIBALLOC_DEBUG
        mos_debug("Melting tag right into available memory. This was was %zu, becomes %zu (%zu)", tag->real_size,
                  tag->split_right->real_size + tag->real_size, tag->split_right->real_size);
#endif
        tag = absorb_right(tag);
    }

    // Where is it going back to?
    int index = getexp(tag->real_size - sizeof(struct boundary_tag));
    if (index < MINEXP)
        index = MINEXP;

    // A whole, empty block?
    if ((tag->split_left == NULL) && (tag->split_right == NULL))
    {

        if (l_completePages[index] == MAXCOMPLETE)
        {
            // Too many standing by to keep. Free this one.
            unsigned int pages = tag->real_size / l_pageSize;

            if ((tag->real_size % l_pageSize) != 0)
                pages += 1;
            if (pages < l_pageCount)
                pages = l_pageCount;

            liballoc_page_free(tag, pages);

#if MOS_MM_LIBALLOC_DEBUG
            l_allocated -= pages * l_pageSize;
            mos_debug("Resource freeing %p of %i pages", (void *) tag, pages);
            dump_array();
#endif
#if MOS_MM_LIBALLOC_LOCKS
            liballoc_unlock();
#endif
            return;
        }

        l_completePages[index] += 1; // Increase the count of complete pages.
    }

    insert_tag(tag, index);

#if MOS_MM_LIBALLOC_DEBUG
    mos_debug("Returning tag with %zu bytes (requested %zu bytes), which has exponent: %i", tag->real_size, tag->size, index);
    dump_array();
#endif
#if MOS_MM_LIBALLOC_LOCKS
    liballoc_unlock();
#endif
}

void *liballoc_calloc(size_t nobj, size_t size)
{
    MOS_ASSERT(l_initialized);
    int real_size = nobj * size;
    void *p = liballoc_malloc(real_size);
    memset(p, 0, real_size);
    return p;
}

void *liballoc_realloc(void *p, size_t size)
{
    MOS_ASSERT(l_initialized);
    if (size == 0)
    {
        // ! The behavior of realloc() when size is equal to zero, and ptr is not NULL, is glibc specific;
        // ! other implementations may return NULL, and set errno.
        // !! Portable POSIX programs should avoid it (realloc(3p)).
        liballoc_free(p);
        return NULL;
    }

    if (p == NULL)
        return liballoc_malloc(size);

#if MOS_MM_LIBALLOC_LOCKS
    liballoc_lock();
#endif
    struct boundary_tag *tag = (struct boundary_tag *) ((unsigned int) p - sizeof(struct boundary_tag));
    size_t real_size = tag->size;
#if MOS_MM_LIBALLOC_LOCKS
    liballoc_unlock();
#endif

    if (real_size > size)
        real_size = size;

    void *ptr = liballoc_malloc(size);
    memcpy(ptr, p, real_size);
    liballoc_free(p);

    return ptr;
}
