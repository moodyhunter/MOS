// SPDX-Licence-Identifier: BSD-3-Clause

#pragma once

#include "mos/types.h"

// This is a boundary tag which is prepended to the page or section of a page which we have allocated.
// It is used to identify valid memory blocks that the application is trying to free.
struct boundary_tag
{
    unsigned int magic;
    size_t size;      // Requested size.
    size_t real_size; // Actual size.
    int index;        // Location in the page table.

    struct boundary_tag *split_left;  // Linked-list info for broken pages.
    struct boundary_tag *split_right; // The same.

    struct boundary_tag *next; // Linked list info.
    struct boundary_tag *prev; // Linked list info.
};

#if MOS_MM_LIBALLOC_LOCKS
/** This function is supposed to lock the memory data structures. It
 * could be as simple as disabling interrupts or acquiring a spinlock.
 * It's up to you to decide.
 *
 * \return 0 if the lock was acquired successfully. Anything else is failure.
 */
extern int liballoc_lock();

/** This function unlocks what was previously locked by the liballoc_lock
 * function.  If it disabled interrupts, it enables interrupts. If it
 * had acquiried a spinlock, it releases the spinlock. etc.
 *
 * \return 0 if the lock was successfully released.
 */
extern int liballoc_unlock();
#endif

void liballoc_init(int page_size);

void *liballoc_malloc(size_t);
void *liballoc_realloc(void *, size_t);
void *liballoc_calloc(size_t, size_t);
void liballoc_free(void *);
