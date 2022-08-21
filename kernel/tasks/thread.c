// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/thread.h"

#include "lib/hashmap.h"
#include "lib/stack.h"
#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_type.h"

#define THREAD_HASHTABLE_SIZE 4096

// TODO: make this configurable
#define THREAD_STACK_SIZE 1 MB

static u32 thread_stack_pages = 0;
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
    thread_stack_pages = THREAD_STACK_SIZE / mos_platform.mm_page_size;
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

thread_id_t create_thread(process_id_t owner_pid, thread_flags_t flags, thread_entry_t entry, void *arg)
{
    thread_t *thread = kmalloc(sizeof(thread_t));
    thread->thread_id = new_thread_id();
    thread->owner_pid = owner_pid;
    thread->entry_point = entry;
    thread->arg = arg;
    thread->status = TASK_STATUS_READY;
    thread->flags = flags;

    // allcate stack for the thread
    void *page = kpage_alloc(thread_stack_pages);
    stack_init(&thread->stack, page, THREAD_STACK_SIZE);

    if (flags & THREAD_FLAG_USERMODE)
        mos_platform.mm_page_set_flags(page, thread_stack_pages, VM_PRESENT | VM_WRITABLE | VM_USERMODE);

    x86_context_t *ctx = stack_grow(&thread->stack, sizeof(x86_context_t));
    memset(ctx, 0, sizeof(x86_context_t));
    ctx->eip = (reg_t) entry;

    // original stack head before pushing the context
    ctx->esp = (reg_t) thread->stack.base;
    ctx->cs = (reg_t) 0x08; // !! kernel code segment ?
    ctx->ds = (reg_t) 0x10; // !! kernel data segment ?
    ctx->es = (reg_t) 0x10; // !! kernel data segment ?
    ctx->fs = (reg_t) 0x10; // !! kernel data segment ?
    ctx->gs = (reg_t) 0x10; // !! kernel data segment ?
    ctx->ss = (reg_t) 0x10; // !! kernel stack ?

    hashmap_put(thread_table, &thread->thread_id, thread);
    return thread->thread_id;
}

thread_t *get_thread(thread_id_t tid)
{
    return hashmap_get(thread_table, &tid);
}
