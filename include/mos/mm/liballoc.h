// SPDX-License-Identifier: BSD-3-Clause
#pragma once

/**  Durand's Amazing Super Duper Memory functions.  */

#include "mos/types.h"

#if MOS_MM_LIBALLOC_LOCKS
/** This function is supposed to lock the memory data structures.
 * It could be as simple as disabling interrupts or acquiring a spinlock.
 * It's up to you to decide.
 *
 * \return 0 if the lock was acquired successfully. Anything else is failure.
 */
extern int liballoc_lock();

/** This function unlocks what was previously locked by the liballoc_lock function.
 * If it disabled interrupts, it enables interrupts. If it had acquiried a spinlock,
 * it releases the spinlock. etc.
 *
 * \return 0 if the lock was successfully released.
 */
extern int liballoc_unlock();
#endif

void liballoc_dump();
void liballoc_init(size_t page_size);
void *liballoc_malloc(size_t size);
void *liballoc_realloc(void *ptr, size_t size);
void *liballoc_calloc(size_t object, size_t n_objects);
void liballoc_free(const void *ptr);
