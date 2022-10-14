// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/thread.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/stack.h"
#include "mos/kconfig.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_type.h"
#include "mos/x86/tasks/context.h"

#define THREAD_HASHTABLE_SIZE 4096

// TODO: make this configurable
#define THREAD_STACK_SIZE 1 MB

static u32 thread_stack_npages = 0;
hashmap_t *thread_table;

static hash_t hashmap_thread_hash(const void *key)
{
    return (hash_t){ .hash = ((thread_id_t *) key)->thread_id };
}

static int hashmap_thread_equal(const void *key1, const void *key2)
{
    return ((thread_id_t *) key1)->thread_id == ((thread_id_t *) key2)->thread_id;
}

void thread_init()
{
    thread_table = kmalloc(sizeof(hashmap_t));
    memset(thread_table, 0, sizeof(hashmap_t));
    hashmap_init(thread_table, THREAD_HASHTABLE_SIZE, hashmap_thread_hash, hashmap_thread_equal);
    thread_stack_npages = THREAD_STACK_SIZE / MOS_PAGE_SIZE;
}

void thread_deinit()
{
    hashmap_deinit(thread_table);
    kfree(thread_table);
}

static thread_id_t new_thread_id()
{
    static thread_id_t next = { 1 };
    return (thread_id_t){ next.thread_id++ };
}

thread_t *create_thread(process_t *owner, thread_flags_t flags, thread_entry_t entry, void *arg)
{
    thread_t *thread = kmalloc(sizeof(thread_t));
    thread->id = new_thread_id();
    thread->owner = owner;
    thread->status = THREAD_STATUS_READY;
    thread->flags = flags;

    // allcate stack for the thread
    void *stack_page = kpage_alloc(thread_stack_npages, PGALLOC_NONE);
    stack_init(&thread->stack, stack_page, THREAD_STACK_SIZE);

    vm_flags stack_flags = VM_WRITE;
    if (flags & THREAD_FLAG_USERMODE)
        stack_flags |= VM_USERMODE;

    mos_platform->mm_map_kvaddr(owner->pagetable, (uintptr_t) stack_page, (uintptr_t) stack_page, thread_stack_npages, stack_flags);
    mos_platform->context_setup(thread, entry, arg);

    hashmap_put(thread_table, &thread->id, thread);
    return thread;
}

thread_t *get_thread(thread_id_t tid)
{
    return hashmap_get(thread_table, &tid);
}
