// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/thread.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/stack.h"
#include "mos/kconfig.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging.h"
#include "mos/platform/platform.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_type.h"
#include "mos/x86/tasks/context.h"

static u32 STACK_NPAGES = 0;
hashmap_t *thread_table;

static hash_t hashmap_thread_hash(const void *key)
{
    return (hash_t){ .hash = *(tid_t *) key };
}

static int hashmap_thread_equal(const void *key1, const void *key2)
{
    return *(tid_t *) key1 == *(tid_t *) key2;
}

void thread_init()
{
    thread_table = kmalloc(sizeof(hashmap_t));
    memset(thread_table, 0, sizeof(hashmap_t));
    hashmap_init(thread_table, MOS_THREAD_STACK_SIZE, hashmap_thread_hash, hashmap_thread_equal);
    STACK_NPAGES = MOS_THREAD_STACK_SIZE / MOS_PAGE_SIZE;
}

void thread_deinit()
{
    hashmap_deinit(thread_table);
    kfree(thread_table);
}

static tid_t new_thread_id()
{
    static tid_t next = 1;
    return (tid_t){ next++ };
}

thread_t *create_thread(process_t *owner, thread_flags_t tflags, thread_entry_t entry, void *arg)
{
    thread_t *t = kmalloc(sizeof(thread_t));
    t->magic[0] = 'T';
    t->magic[1] = 'H';
    t->magic[2] = 'R';
    t->magic[3] = 'D';
    t->tid = new_thread_id();
    t->owner = owner;
    t->status = THREAD_STATUS_READY;
    t->flags = tflags;

    vm_flags sflags = VM_READ | VM_WRITE;
    pgalloc_hints hints = PGALLOC_HINT_DEFAULT;

    if (tflags & THREAD_FLAG_USERMODE)
        sflags |= VM_USER, hints = PGALLOC_HINT_USERSPACE;

    // allcate stack for the thread, in the kernel space
    const vmblock_t stack_block = mos_platform->mm_alloc_pages(current_cpu->pagetable, STACK_NPAGES, hints, sflags);
    void *const stack = (void *) stack_block.vaddr;
    stack_init(&t->stack, stack, MOS_THREAD_STACK_SIZE);

    // copy the stack mappping to the process address space
    vmblock_t blk = mos_platform->mm_copy_maps(current_cpu->pagetable, (uintptr_t) stack, owner->pagetable, (uintptr_t) stack, stack_block.pages);
    mos_platform->mm_flag_pages(owner->pagetable, blk.vaddr, blk.pages, sflags);

    process_attach_mmap(owner, blk, VMTYPE_STACK);
    mos_platform->context_setup(t, entry, arg);

    hashmap_put(thread_table, &t->tid, t);
    process_attach_thread(owner, t);

    // unmap the stack from the kernel space
    mos_platform->mm_unmap_pages(current_cpu->pagetable, stack_block.vaddr, stack_block.pages);
    return t;
}

thread_t *get_thread(tid_t tid)
{
    return hashmap_get(thread_table, &tid);
}
