// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/kconfig.h"
#include "mos/mos_global.h"
#include "mos/types.h"

#define PER_CPU_DECLARE(type, name)                                                                                                             \
    struct                                                                                                                                      \
    {                                                                                                                                           \
        type percpu_value[MOS_MAX_CPU_COUNT];                                                                                                   \
    } name

#define per_cpu(var) (&(var.percpu_value[mos_platform->current_cpu_id()]))

typedef void (*irq_handler)(u32 irq);
typedef void (*thread_entry_t)(void *arg);

typedef struct _thread thread_t;

typedef enum
{
    VM_NONE = 0,
    VM_READ = 1 << 0,
    VM_WRITE = 1 << 1,
    VM_USER = 1 << 2,
    VM_WRITE_THROUGH = 1 << 3,
    VM_CACHE_DISABLED = 1 << 4,
    VM_GLOBAL = 1 << 5,
    VM_EXEC = 1 << 6,
} vm_flags;

// indicates which type of page we are allocating
typedef enum
{
    PGALLOC_HINT_DEFAULT = 0 << 0,
    PGALLOC_HINT_KHEAP = 1 << 0,
    PGALLOC_HINT_USERSPACE = 1 << 1,
} pgalloc_hints;

typedef enum
{
    VMTYPE_APPCODE,
    VMTYPE_APPDATA,
    VMTYPE_STACK,
    VMTYPE_FILE,
} vm_type;

typedef struct
{
    u32 id;
    thread_t *thread;
    uintptr_t scheduler_stack;
} cpu_t;

typedef struct
{
    uintptr_t vaddr;
    uintptr_t paddr;
    size_t size_bytes;
    bool available;
} memblock_t;

typedef struct
{
    uintptr_t vaddr;
    uintptr_t paddr; // probably non-contiguous
    size_t pages;
    vm_flags flags;
} vmblock_t;

typedef struct
{
    vmblock_t vm;
    vm_type type;
} proc_vmblock_t;

typedef struct
{
    struct
    {
        const uintptr_t code_start;
        const uintptr_t code_end;
        const uintptr_t rodata_start;
        const uintptr_t rodata_end;
        const uintptr_t rw_start;
        const uintptr_t rw_end;
    } regions;

    u32 num_cpus;
    u32 boot_cpu_id;
    PER_CPU_DECLARE(cpu_t, cpu);

    paging_handle_t kernel_pg;

    void noreturn (*const shutdown)(void);

    // cpu
    void (*const halt_cpu)(void);
    u32 (*const current_cpu_id)(void);

    // interrupt
    void (*const interrupt_enable)(void);
    void (*const interrupt_disable)(void);
    bool (*const irq_handler_install)(u32 irq, irq_handler handler);
    void (*const irq_handler_remove)(u32 irq, irq_handler handler);

    // memory management
    paging_handle_t (*const mm_create_pagetable)();
    void (*const mm_destroy_pagetable)(paging_handle_t table);

    vmblock_t (*const mm_alloc_pages)(paging_handle_t table, size_t n, pgalloc_hints pg_flags, vm_flags vm_flags);
    vmblock_t (*const mm_alloc_pages_at)(paging_handle_t table, uintptr_t addr, size_t n, vm_flags vflags);
    bool (*const mm_free_pages)(paging_handle_t table, uintptr_t vaddr, size_t n);
    // void (*const mm_flag_pages)(paging_handle_t table, uintptr_t vaddr, size_t n, vm_flags flags);
    vmblock_t (*const mm_map_kvaddr)(paging_handle_t table, uintptr_t virt, uintptr_t kvaddr, size_t n, vm_flags flags);
    void (*const mm_unmap)(paging_handle_t table, uintptr_t virt, size_t n);

    // process management
    void (*const context_setup)(thread_t *thread, thread_entry_t entry, void *arg);
    void (*const switch_to_scheduler)(uintptr_t *old_stack, uintptr_t new_stack);
    void (*const switch_to_thread)(uintptr_t *old_stack, thread_t *new_thread);
} mos_platform_t;

extern mos_platform_t *const mos_platform;

#define current_cpu     per_cpu(mos_platform->cpu)
#define current_thread  (current_cpu->thread)
#define current_process (current_thread->owner)

extern void mos_start_kernel(const char *cmdline);
extern void mos_kernel_mm_init(void);
