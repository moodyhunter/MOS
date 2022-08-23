// SPDX-License-Identifier: BSD-3-Clause

#include "mos/mm/liballoc.h"

#include "lib/string.h"
#include "mos/mm/paging.h"
#include "mos/printk.h"

#define VERSION "1.1"

// This is the byte alignment that memory must be allocated on. IMPORTANT for GTK and other stuff.
#define ALIGNMENT 16 // or 4

// unsigned char[16], or unsigned short
#define ALIGN_TYPE char

// Alignment information is stored right before the pointer. This is the number of bytes of information stored there.
#define ALIGN_INFO sizeof(ALIGN_TYPE) * 16

#define USE_CASE1
#define USE_CASE2
#define USE_CASE3
#define USE_CASE4
#define USE_CASE5

/** This macro will conveniently align our pointer upwards */
#define LIBALLOC_ALIGN_PTR(ptr)                                                                                                                 \
    if (ALIGNMENT > 1)                                                                                                                          \
    {                                                                                                                                           \
        uintptr_t diff;                                                                                                                         \
        ptr = (void *) ((uintptr_t) ptr + ALIGN_INFO);                                                                                          \
        diff = (uintptr_t) ptr & (ALIGNMENT - 1);                                                                                               \
        if (diff != 0)                                                                                                                          \
        {                                                                                                                                       \
            diff = ALIGNMENT - diff;                                                                                                            \
            ptr = (void *) ((uintptr_t) ptr + diff);                                                                                            \
        }                                                                                                                                       \
        *((ALIGN_TYPE *) ((uintptr_t) ptr - ALIGN_INFO)) = diff + ALIGN_INFO;                                                                   \
    }

#define LIBALLOC_UNALIGN_PTR(ptr)                                                                                                               \
    if (ALIGNMENT > 1)                                                                                                                          \
    {                                                                                                                                           \
        uintptr_t diff = *((ALIGN_TYPE *) ((uintptr_t) ptr - ALIGN_INFO));                                                                      \
        if (diff < (ALIGNMENT + ALIGN_INFO))                                                                                                    \
            ptr = (void *) ((uintptr_t) ptr - diff);                                                                                            \
    }

#define LIBALLOC_MAGIC 0xaabbccdd
#define LIBALLOC_DEAD  0xdeaddead

// A structure found at the top of all system allocated memory blocks. It details the usage of the memory block.
typedef struct liballoc_major_struct
{
    struct liballoc_major_struct *prev;  // Linked list information.
    struct liballoc_major_struct *next;  // Linked list information.
    size_t pages;                        // The number of pages in the block.
    size_t size;                         // bytes in the block.
    size_t usage;                        // bytes used in the block.
    struct liballoc_minor_struct *first; // A pointer to the first allocated memory in the block.
} liballoc_block_t;

// This is a structure found at the beginning of all sections in a major block which were allocated by a malloc, calloc, realloc call.
typedef struct liballoc_minor_struct
{
    struct liballoc_minor_struct *prev;  // Linked list information.
    struct liballoc_minor_struct *next;  // Linked list information.
    struct liballoc_major_struct *block; // The owning block. A pointer to the major structure.
    u32 magic;                           // A magic number to idenfity correctness.
    size_t size;                         // The size of the memory allocated. Could be 1 byte or more.
    size_t req_size;                     // The size of memory requested.
} liballoc_minor_t;

static liballoc_block_t *l_memroot = NULL; // The root memory block acquired from the system.
static liballoc_block_t *l_bestbet = NULL; // The major with the most free memory.

static size_t l_page_size = 0;         // The size of an individual page. Set up in liballoc_init.
static size_t l_alloc_n_page_once = 0; // The number of pages to request per chunk. Set up in liballoc_init.

static size_t l_mem_allocated = 0;     // Running total of allocated memory.
static size_t l_mem_inuse = 0;         // Running total of used memory.
static size_t l_warnings = 0;          // Number of warnings encountered
static size_t l_errors = 0;            // Number of actual errors
static size_t l_possible_overruns = 0; // Number of possible overruns

#if MOS_MM_LIBALLOC_DEBUG
static void liballoc_dump()
{
    pr_info("--------------- Memory data ---------------");
    pr_info("Total Memory Allocated: %zu bytes", l_mem_allocated);
    pr_info("Memory Used (malloc'ed): %zu bytes", l_mem_inuse);
    pr_info("Possible Overruns: %zu", l_possible_overruns);
    pr_info("emitted %zu warning(s) and %zu error(s)", l_warnings, l_errors);

    liballoc_block_t *maj = l_memroot;
    liballoc_minor_t *min = NULL;

    while (maj != NULL)
    {
        pr_info("liballoc: %p: total = %zu, used = %zu", (void *) maj, maj->size, maj->usage);

        min = maj->first;
        while (min != NULL)
        {
            pr_info("liballoc:    %p: %zu bytes", (void *) min, min->size);
            min = min->next;
        }

        maj = maj->next;
    }
}
#endif

static liballoc_block_t *allocate_new_pages_for(unsigned int size)
{
    // This is how much space is required.
    u32 st = size + sizeof(liballoc_block_t) + sizeof(liballoc_minor_t);

    // Perfect amount of space?
    if ((st % l_page_size) == 0)
        st = st / (l_page_size);
    else
        st = st / (l_page_size) + 1;
    // No, add the buffer.

    // Make sure it's >= the minimum size.
    if (st < l_alloc_n_page_once)
        st = l_alloc_n_page_once;

    liballoc_block_t *maj = (liballoc_block_t *) kpage_alloc(st);
    if (maj == NULL)
    {
        l_warnings += 1;
        mos_warn("liballoc: WARNING: liballoc_alloc(%i) returns NULL", st);
        return NULL;
    }

    maj->prev = NULL;
    maj->next = NULL;
    maj->pages = st;
    maj->size = st * l_page_size;
    maj->usage = sizeof(liballoc_block_t);
    maj->first = NULL;

    l_mem_allocated += maj->size;

#if MOS_MM_LIBALLOC_DEBUG
    pr_info("liballoc: Allocated %u pages (%zu bytes) at %p for %u bytes to be used.", st, maj->size, (void *) maj, size);
    pr_info("liballoc: Total memory usage = %i KB", (int) ((l_mem_allocated / (1024))));
#endif

    return maj;
}

void liballoc_init(size_t page_size)
{
    l_page_size = page_size;
    l_alloc_n_page_once = 16;

    MOS_ASSERT_X(l_memroot == NULL, "liballoc_init() called twice");

#if MOS_MM_LIBALLOC_DEBUG
    pr_info("liballoc: initialization of liballoc " VERSION "");
    // atexit(liballoc_dump);
#endif

    // This is the first time we are being used.
    l_memroot = allocate_new_pages_for(sizeof(liballoc_block_t));
    if (l_memroot == NULL)
    {
#if MOS_MM_LIBALLOC_LOCKS
        liballoc_unlock();
#endif
#if MOS_MM_LIBALLOC_DEBUG
        pr_info("liballoc: initial l_memRoot initialization failed");
#endif
    }

#if MOS_MM_LIBALLOC_DEBUG
    pr_info("liballoc: set up first memory major %p", (void *) l_memroot);
#endif
}

void *liballoc_malloc(size_t req_size)
{
    unsigned long size = req_size;

    // For alignment, we adjust size so there's enough space to align.
    if (ALIGNMENT > 1)
    {
        size += ALIGNMENT + ALIGN_INFO;
    }
    // So, ideally, we really want an alignment of 0 or 1 in order to save space.

#if MOS_MM_LIBALLOC_LOCKS
    liballoc_lock();
#endif

    if (size == 0)
    {
        mos_warn("liballoc: liballoc_malloc(0) called.");
        l_warnings += 1;
#if MOS_MM_LIBALLOC_LOCKS
        liballoc_unlock();
#endif
        return liballoc_malloc(1);
    }

    MOS_ASSERT_X(l_memroot, "liballoc: liballoc_malloc() called before liballoc_init().");

    // Now we need to bounce through every major and find enough space....

    int startedBet = 0;
    size_t bestSize = 0;
    liballoc_block_t *block = l_memroot;

    // Start at the best bet....
    if (l_bestbet != NULL)
    {
        bestSize = l_bestbet->size - l_bestbet->usage;

        if (bestSize > (size + sizeof(liballoc_minor_t)))
        {
            block = l_bestbet;
            startedBet = 1;
        }
    }

    while (block != NULL)
    {
        s64 diff = block->size - block->usage;
        // free memory in the block

        if (bestSize < diff)
        {
            // Hmm.. this one has more memory then our bestBet. Remember!
            l_bestbet = block;
            bestSize = diff;
        }

#ifdef USE_CASE1
        // CASE 1:  There is not enough space in this major block.
        if (diff < (size + sizeof(liballoc_minor_t)))
        {
#if MOS_MM_LIBALLOC_DEBUG
            pr_info("CASE 1: Insufficient space in block %p", (void *) block);
#endif

            // Another major block next to this one?
            if (block->next != NULL)
            {
                block = block->next; // Hop to that one.
                continue;
            }

            if (startedBet == 1) // If we started at the best bet,
            {                    // let's start all over again.
                block = l_memroot;
                startedBet = 0;
                continue;
            }

            // Create a new major block next to this one and...
            block->next = allocate_new_pages_for(size); // next one will be okay.
            if (block->next == NULL)
                break; // no more memory.
            block->next->prev = block;
            block = block->next;

            // .. fall through to CASE 2 ..
        }
#endif

#ifdef USE_CASE2
        // CASE 2: It's a brand new block.
        if (block->first == NULL)
        {
            block->first = (liballoc_minor_t *) ((uintptr_t) block + sizeof(liballoc_block_t));

            block->first->magic = LIBALLOC_MAGIC;
            block->first->prev = NULL;
            block->first->next = NULL;
            block->first->block = block;
            block->first->size = size;
            block->first->req_size = req_size;
            block->usage += size + sizeof(liballoc_minor_t);

            l_mem_inuse += size;

            void *p = (void *) ((uintptr_t) (block->first) + sizeof(liballoc_minor_t));

            LIBALLOC_ALIGN_PTR(p);

#if MOS_MM_LIBALLOC_LOCKS
            liballoc_unlock(); // release the lock
#endif

#if MOS_MM_LIBALLOC_DEBUG
            pr_info("liballoc: allocating %lu bytes at %p", size, p);
#endif
            return p;
        }

#endif

        // CASE 3: Block in use and enough space at the start of the block.
#ifdef USE_CASE3
        {
            s64 diff = (uintptr_t) block->first - (uintptr_t) block;
            diff -= sizeof(liballoc_block_t);

            if (diff >= (size + sizeof(liballoc_minor_t)))
            {
                // Yes, space in front. Squeeze in.
                block->first->prev = (liballoc_minor_t *) ((uintptr_t) block + sizeof(liballoc_block_t));
                block->first->prev->next = block->first;
                block->first = block->first->prev;

                block->first->magic = LIBALLOC_MAGIC;
                block->first->prev = NULL;
                block->first->block = block;
                block->first->size = size;
                block->first->req_size = req_size;
                block->usage += size + sizeof(liballoc_minor_t);

                l_mem_inuse += size;

                void *p = (void *) ((uintptr_t) (block->first) + sizeof(liballoc_minor_t));
                LIBALLOC_ALIGN_PTR(p);

#if MOS_MM_LIBALLOC_LOCKS
                liballoc_unlock(); // release the lock
#endif
#if MOS_MM_LIBALLOC_DEBUG
                pr_info("liballoc: allocating %lu bytes at %p", size, p);
#endif
                return p;
            }
        }
#endif

#ifdef USE_CASE4
        // CASE 4: There is enough space in this block. But is it contiguous?
        liballoc_minor_t *section = block->first;

        // Looping within the block now...
        while (section != NULL)
        {
            // CASE 4.1: End of a section in a block. check for enough space from last and end?
            if (section->next == NULL)
            {
                // the rest of this block is free...  is it big enough?
                size_t size_left = block->size;                        // size of the block
                size_left -= (uintptr_t) section - (uintptr_t) block;  // minus the area before this section
                size_left -= sizeof(liballoc_minor_t) + section->size; // minus this section

                // if there's still enough space
                if (size_left >= sizeof(liballoc_minor_t) + size)
                {
                    // yay....
                    section->next = (liballoc_minor_t *) ((uintptr_t) section + sizeof(liballoc_minor_t) + section->size);
                    section->next->prev = section;

                    // go to the next section
                    section = section->next;
                    section->next = NULL;
                    section->magic = LIBALLOC_MAGIC;
                    section->block = block;
                    section->size = size;
                    section->req_size = req_size;

                    block->usage += size + sizeof(liballoc_minor_t);

                    l_mem_inuse += size;

                    void *p = (void *) ((uintptr_t) section + sizeof(liballoc_minor_t));
                    LIBALLOC_ALIGN_PTR(p);

#if MOS_MM_LIBALLOC_LOCKS
                    liballoc_unlock(); // release the lock
#endif
#if MOS_MM_LIBALLOC_DEBUG
                    pr_info("liballoc: allocating %lu bytes at %p", size, p);
#endif
                    return p;
                }
            }

            // CASE 4.2: Is there space between two minors?
            if (section->next != NULL)
            {
                // is the difference between here and next big enough?
                s64 diff = (uintptr_t) (section->next);
                diff -= (uintptr_t) section;
                diff -= sizeof(liballoc_minor_t);
                diff -= section->size;
                // minus our existing usage.

                if (diff >= (size + sizeof(liballoc_minor_t)))
                {
                    // yay......
                    liballoc_minor_t *new_min = (liballoc_minor_t *) ((uintptr_t) section + sizeof(liballoc_minor_t) + section->size);

                    new_min->magic = LIBALLOC_MAGIC;
                    new_min->next = section->next;
                    new_min->prev = section;
                    new_min->size = size;
                    new_min->req_size = req_size;
                    new_min->block = block;
                    section->next->prev = new_min;
                    section->next = new_min;
                    block->usage += size + sizeof(liballoc_minor_t);

                    l_mem_inuse += size;

                    void *p = (void *) ((uintptr_t) new_min + sizeof(liballoc_minor_t));
                    LIBALLOC_ALIGN_PTR(p);

#if MOS_MM_LIBALLOC_LOCKS
                    liballoc_unlock(); // release the lock
#endif
#if MOS_MM_LIBALLOC_DEBUG
                    pr_info("liballoc: allocating %lu bytes at %p", size, p);
#endif
                    return p;
                }
            } // min->next != NULL

            section = section->next;
        } // while min != NULL ...

#endif

#ifdef USE_CASE5
        // CASE 5: Block full! Ensure next block and loop.
        if (block->next == NULL)
        {
#if MOS_MM_LIBALLOC_DEBUG
            pr_info("CASE 5: block full");
#endif

            if (startedBet == 1)
            {
                block = l_memroot;
                startedBet = 0;
                continue;
            }

            // we've run out. we need more...
            block->next = allocate_new_pages_for(size); // next one guaranteed to be okay
            if (block->next == NULL)
                break; //  uh oh,  no more memory.....
            block->next->prev = block;
        }

#endif

        block = block->next;
    } // while (maj != NULL)

#if MOS_MM_LIBALLOC_LOCKS
    liballoc_unlock(); // release the lock
#endif

    mos_warn("No memory available.");

#if MOS_MM_LIBALLOC_DEBUG
    liballoc_dump();
#endif

    return NULL;
}

void liballoc_free(const void *ptr)
{
    liballoc_minor_t *min;
    liballoc_block_t *maj;

    if (ptr == NULL)
    {
        l_warnings += 1;
        mos_warn("liballoc: free(NULL) called");
        return;
    }

    LIBALLOC_UNALIGN_PTR(ptr);

#if MOS_MM_LIBALLOC_LOCKS
    liballoc_lock(); // lockit
#endif

    min = (liballoc_minor_t *) ((uintptr_t) ptr - sizeof(liballoc_minor_t));

    if (min->magic != LIBALLOC_MAGIC)
    {
        l_errors += 1;

        // Check for overrun errors. For all bytes of LIBALLOC_MAGIC
        if (((min->magic & 0xFFFFFF) == (LIBALLOC_MAGIC & 0xFFFFFF)) || ((min->magic & 0xFFFF) == (LIBALLOC_MAGIC & 0xFFFF)) ||
            ((min->magic & 0xFF) == (LIBALLOC_MAGIC & 0xFF)))
        {
            l_possible_overruns += 1;
            mos_warn("liballoc: ERROR: Possible 1-3 byte overrun for magic %x != %x", min->magic, LIBALLOC_MAGIC);
        }

        if (min->magic == LIBALLOC_DEAD)
        {
            mos_warn("liballoc: multiple free() attempt on %p", ptr);
        }
        else
        {
            mos_warn("liballoc: bad free(%p) called.", ptr);
        }
#if MOS_MM_LIBALLOC_LOCKS
        // being lied to...
        liballoc_unlock(); // release the lock
#endif
        return;
    }

    maj = min->block;
    l_mem_inuse -= min->size;
    maj->usage -= (min->size + sizeof(liballoc_minor_t));
    min->magic = LIBALLOC_DEAD; // No mojo.

    if (min->next != NULL)
        min->next->prev = min->prev;
    if (min->prev != NULL)
        min->prev->next = min->next;

    if (min->prev == NULL)
        maj->first = min->next;
    // Might empty the block. This was the first
    // minor.

    // We need to clean up after the majors now....

    if (maj->first == NULL) // Block completely unused.
    {
        if (l_memroot == maj)
            l_memroot = maj->next;
        if (l_bestbet == maj)
            l_bestbet = NULL;
        if (maj->prev != NULL)
            maj->prev->next = maj->next;
        if (maj->next != NULL)
            maj->next->prev = maj->prev;
        l_mem_allocated -= maj->size;

        kpage_free(maj, maj->pages);
    }
    else
    {
        if (l_bestbet != NULL)
        {
            int bestSize = l_bestbet->size - l_bestbet->usage;
            int majSize = maj->size - maj->usage;

            if (majSize > bestSize)
                l_bestbet = maj;
        }
    }

#if MOS_MM_LIBALLOC_LOCKS
    liballoc_unlock(); // release the lock
#endif
}

void *liballoc_calloc(size_t nobj, size_t size)
{
    int real_size = nobj * size;
    void *p = liballoc_malloc(real_size);
    memset(p, 0, real_size);
    return p;
}

void *liballoc_realloc(void *p, size_t size)
{

    // Honour the case of size == 0 => free old and return NULL
    if (size == 0)
    {
        liballoc_free(p);
        return NULL;
    }

    // In the case of a NULL pointer, return a simple malloc.
    if (p == NULL)
        return liballoc_malloc(size);

    // Unalign the pointer if required.

    void *ptr = p;
    LIBALLOC_UNALIGN_PTR(ptr);

#if MOS_MM_LIBALLOC_LOCKS
    liballoc_lock(); // lockit
#endif

    liballoc_minor_t *min = (liballoc_minor_t *) ((uintptr_t) ptr - sizeof(liballoc_minor_t));

    // Ensure it is a valid structure.
    if (min->magic != LIBALLOC_MAGIC)
    {
        l_errors += 1;

        // Check for overrun errors. For all bytes of LIBALLOC_MAGIC
        if (((min->magic & 0xFFFFFF) == (LIBALLOC_MAGIC & 0xFFFFFF)) || ((min->magic & 0xFFFF) == (LIBALLOC_MAGIC & 0xFFFF)) ||
            ((min->magic & 0xFF) == (LIBALLOC_MAGIC & 0xFF)))
        {
            l_possible_overruns += 1;
            mos_warn("liballoc: Possible 1-3 byte overrun for magic %x != %x", min->magic, LIBALLOC_MAGIC);
        }

        if (min->magic == LIBALLOC_DEAD)
        {
            mos_warn("liballoc: multiple free() attempt on %p from %p.", ptr, __builtin_return_address(0));
        }
        else
        {
            mos_warn("liballoc: Bad free(%p) called", ptr);
        }
#if MOS_MM_LIBALLOC_LOCKS
        // being lied to...
        liballoc_unlock(); // release the lock
#endif
        return NULL;
    }

    // Definitely a memory block.

    unsigned int real_size = min->req_size;

    if (real_size >= size)
    {
        min->req_size = size;
#if MOS_MM_LIBALLOC_LOCKS
        liballoc_unlock();
#endif
        return p;
    }
#if MOS_MM_LIBALLOC_LOCKS
    liballoc_unlock();
#endif

    // If we got here then we're reallocating to a block bigger than us.
    ptr = liballoc_malloc(size); // We need to allocate new memory
    memcpy(ptr, p, real_size);
    liballoc_free(p);

    return ptr;
}
