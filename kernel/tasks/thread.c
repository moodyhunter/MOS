// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/thread.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/memops.h"
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

    t->name = duplicate_string(name, strlen(name));

    // Kernel stack
    const pgalloc_hints kstack_hint = (tmode == THREAD_MODE_KERNEL) ? PGALLOC_HINT_KHEAP : PGALLOC_HINT_STACK;
    const vmblock_t kstack_blk = platform_mm_alloc_pages(owner->pagetable, MOS_STACK_PAGES_KERNEL, kstack_hint, VM_RW);
    stack_init(&t->kernel_stack, (void *) kstack_blk.vaddr, kstack_blk.npages * MOS_PAGE_SIZE);
    process_attach_mmap(owner, kstack_blk, VMTYPE_KSTACK, MMAP_DEFAULT);

    if (tmode == THREAD_MODE_USER)
    {
        // allcate stack for the thread
        // TODO: change [platform_mm_alloc_pages] to [mm_alloc_zeroed_pages] once
        const vmblock_t ustack_blk = platform_mm_alloc_pages(owner->pagetable, MOS_STACK_PAGES_USER, PGALLOC_HINT_STACK, VM_USER_RW);
        stack_init(&t->stack, (void *) ustack_blk.vaddr, MOS_STACK_PAGES_USER * MOS_PAGE_SIZE);
        process_attach_mmap(owner, ustack_blk, VMTYPE_STACK, MMAP_DEFAULT);

        // we cannot access the stack until we switch to its address space, so we
        // map the stack into current kernel's address space, making a proxy stack for it
        // TODO: any way to avoid this?
        // TODO: possibly jumps back to a kernel function to do this (when switching to the correct address space)
        const vmblock_t ustack_proxy = mm_map_proxy_space(owner->pagetable, ustack_blk.vaddr, ustack_blk.npages);
        downwards_stack_t proxy_stack;
        stack_init(&proxy_stack, (void *) ustack_proxy.vaddr, ustack_proxy.npages * MOS_PAGE_SIZE);
        platform_context_setup(t, &proxy_stack, entry, arg);
        t->stack.head -= proxy_stack.top - proxy_stack.head;
        mm_unmap_proxy_space(ustack_proxy);
    }
    else
    {
        // kernel thread
        stack_init(&t->stack, NULL, 0);
        platform_context_setup(t, &t->kernel_stack, entry, arg);
    }

    hashmap_put(thread_table, &t->tid, t);
    process_attach_thread(owner, t);

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

    mos_warn("TODO");
    t->state = THREAD_STATE_DEAD;
}
