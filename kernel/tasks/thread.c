// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/thread.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/memops.h"
#include "mos/mm/paging/paging.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"

#define THREAD_HASHTABLE_SIZE 512

hashmap_t *thread_table;

static hash_t hashmap_thread_hash(const void *key)
{
    return (hash_t){ .hash = *(tid_t *) key };
}

static int hashmap_thread_equal(const void *key1, const void *key2)
{
    return *(tid_t *) key1 == *(tid_t *) key2;
}

static tid_t new_thread_id()
{
    static tid_t next = 1;
    return (tid_t){ next++ };
}

thread_t *thread_allocate(process_t *owner, thread_mode tflags)
{
    thread_t *t = kzalloc(sizeof(thread_t));
    t->magic = THREAD_MAGIC_THRD;
    t->tid = new_thread_id();
    t->owner = owner;
    t->state = THREAD_STATE_CREATED;
    t->mode = tflags;
    t->waiting_condition = NULL;

    return t;
}

void thread_init()
{
    thread_table = kzalloc(sizeof(hashmap_t));
    hashmap_init(thread_table, THREAD_HASHTABLE_SIZE, hashmap_thread_hash, hashmap_thread_equal);
}

void thread_deinit()
{
    hashmap_deinit(thread_table);
    kfree(thread_table);
}

thread_t *thread_new(process_t *owner, thread_mode tmode, const char *name, thread_entry_t entry, void *arg)
{
    thread_t *t = thread_allocate(owner, tmode);

    t->name = strdup(name);

    // Kernel stack
    const pgalloc_hints kstack_hint = (tmode == THREAD_MODE_KERNEL) ? PGALLOC_HINT_KHEAP : PGALLOC_HINT_STACK;
    const vmblock_t kstack_blk = mm_alloc_pages(owner->pagetable, MOS_STACK_PAGES_KERNEL, kstack_hint, VM_RW);
    stack_init(&t->k_stack, (void *) kstack_blk.vaddr, kstack_blk.npages * MOS_PAGE_SIZE);
    process_attach_mmap(owner, kstack_blk, VMTYPE_KSTACK, MMAP_DEFAULT);

    if (tmode == THREAD_MODE_USER)
    {
        // User stack
        const vmblock_t ustack_blk = mm_alloc_zeroed_pages(owner->pagetable, MOS_STACK_PAGES_USER, PGALLOC_HINT_STACK, VM_USER_RW);
        stack_init(&t->u_stack, (void *) ustack_blk.vaddr, MOS_STACK_PAGES_USER * MOS_PAGE_SIZE);
        process_attach_mmap(owner, ustack_blk, VMTYPE_STACK, MMAP_ZERO_ON_DEMAND);
    }
    else
    {
        // kernel thread has no user stack
        stack_init(&t->u_stack, NULL, 0);
    }

    platform_context_setup(t, entry, arg);
    process_attach_thread(owner, t);
    hashmap_put(thread_table, &t->tid, t);

    return t;
}

thread_t *thread_get(tid_t tid)
{
    return hashmap_get(thread_table, &tid);
}

void thread_handle_exit(thread_t *t)
{
    if (!thread_is_valid(t))
        return;

    spinlock_acquire(&t->state_lock);
    t->state = THREAD_STATE_DEAD;
    spinlock_release(&t->state_lock);
    mos_warn("TODO");
}
