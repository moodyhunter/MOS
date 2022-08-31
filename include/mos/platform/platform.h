// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/containers.h"
#include "mos/mos_global.h"
#include "mos/tasks/task_type.h"
#include "mos/types.h"

#define MOS_SYSCALL_INTR 0x88

typedef enum
{
    VM_NONE = 0,
    VM_PRESENT = 1 << 0,
    VM_WRITABLE = 1 << 1,
    VM_USERMODE = 1 << 2,
    VM_WRITE_THROUGH = 1 << 3,
    VM_CACHE_DISABLED = 1 << 4,
} page_flags;

typedef struct
{
    u32 cpu_count;
    u32 bsp_apic_id;
} mos_platform_cpu_info_t;

typedef struct
{
    as_linked_list;
    void (*handler)(u32 irq);
} irq_handler_descriptor_t;

typedef struct
{
    const uintptr_t kernel_start;
    const uintptr_t kernel_end;

    const mos_platform_cpu_info_t *cpu_info;

    void __noreturn (*shutdown)(void);

    // interrupt
    void (*interrupt_enable)(void);
    void (*interrupt_disable)(void);
    void (*halt_cpu)(void);
    bool (*irq_handler_install)(u32 irq, void (*handler)(u32 irq));
    void (*irq_handler_remove)(u32 irq, void (*handler)(u32 irq));

    // memory management
    size_t mm_page_size;
    paging_handle_t kernel_pg;

    void (*mm_pgd_alloc)(paging_handle_t *table);
    void (*mm_pgd_free)(paging_handle_t table);

    void *(*mm_pg_alloc)(paging_handle_t table, size_t n);
    bool (*mm_pg_free)(paging_handle_t table, uintptr_t vaddr, size_t n);
    void (*mm_pg_flag)(paging_handle_t table, uintptr_t vaddr, size_t n, page_flags flags);
    void (*mm_pg_map_to_kvaddr)(paging_handle_t table, uintptr_t virt, uintptr_t kvaddr, size_t n, page_flags flags);
    void (*mm_pg_unmap)(paging_handle_t table, uintptr_t virt, size_t n);

    // process management
    void (*usermode_trampoline)(uintptr_t stack, uintptr_t entry, uintptr_t arg);
    void (*context_switch)(thread_t *from, thread_t *to);
} mos_platform_t;

extern const mos_platform_t mos_platform;

extern void mos_start_kernel(const char *cmdline);
extern void mos_invoke_syscall(u64 syscall_number);
extern void mos_kernel_mm_init(void);
