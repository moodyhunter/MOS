// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/io/io.h>
#include <mos/kconfig.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/mm_types.h>
#include <mos/mos_global.h>
#include <mos/types.h>

#if MOS_CONFIG(MOS_SMP)
#define PER_CPU_DECLARE(type, name)                                                                                                                                      \
    struct name                                                                                                                                                          \
    {                                                                                                                                                                    \
        type percpu_value[MOS_MAX_CPU_COUNT];                                                                                                                            \
    } name

#define per_cpu(var) (&(var.percpu_value[platform_current_cpu_id()]))
#else
#define PER_CPU_DECLARE(type, name)                                                                                                                                      \
    struct name                                                                                                                                                          \
    {                                                                                                                                                                    \
        type percpu_value;                                                                                                                                               \
    } name

#define per_cpu(var) (&(var.percpu_value))
#endif

#define current_cpu     per_cpu(platform_info->cpu)
#define current_thread  (current_cpu->thread)
#define current_process (current_thread->owner)

typedef void (*irq_handler)(u32 irq);

typedef struct _thread thread_t;
typedef struct _page_map page_map_t;

typedef enum
{
    VM_READ = MEM_PERM_READ,   // 1 << 0
    VM_WRITE = MEM_PERM_WRITE, // 1 << 1
    VM_EXEC = MEM_PERM_EXEC,   // 1 << 2

    VM_USER = 1 << 3,
    VM_WRITE_THROUGH = 1 << 4,
    VM_CACHE_DISABLED = 1 << 5,
    VM_GLOBAL = 1 << 6,

    // composite flags (for convenience)
    VM_RW = VM_READ | VM_WRITE,
    VM_USER_RW = VM_USER | VM_RW,
    VM_USER_RO = VM_USER | VM_READ,
} vm_flags;

typedef enum
{
    THREAD_STATE_CREATING, // thread is being created, DO NOT schedule
    THREAD_STATE_CREATED,  // created or forked, but not ever started
    THREAD_STATE_READY,    // thread can be scheduled
    THREAD_STATE_RUNNING,  // thread is currently running
    THREAD_STATE_BLOCKED,  // thread is blocked by a wait condition
    THREAD_STATE_DEAD,     // thread is dead, and will be cleaned up soon by the scheduler
} thread_state_t;

typedef enum
{
    SWITCH_REGULAR = 0,
    SWITCH_TO_NEW_USER_THREAD = 1 << 1,
    SWITCH_TO_NEW_KERNEL_THREAD = 1 << 2,
} switch_flags_t;

#define TARGET_CPU_ALL 0xFF

/**
 * @brief The type of IPI to send
 *
 */
typedef enum
{
    IPI_TYPE_HALT = 0,       // halt the CPU
    IPI_TYPE_INVALIDATE_TLB, // TLB shootdown
    IPI_TYPE_MAX,
} ipi_type_t;

MOS_STATIC_ASSERT(IPI_TYPE_MAX <= (u8) 0xFF, "IPI_TYPE_MAX must fit in a u8");

typedef struct
{
    size_t argc; // size of argv, does not include the terminating NULL
    const char **argv;
} argv_t;

/**
 * @brief A wrapper type for the standard I/O streams
 */
typedef struct
{
    io_t *in, *out, *err;
} stdio_t;

typedef struct
{
    ptr_t pgd;
    spinlock_t *pgd_lock;
    page_map_t *um_page_map;
} paging_handle_t;

typedef struct
{
    ptr_t instruction;
    ptr_t stack;
} __packed thread_context_t;

typedef struct
{
    u32 id;
    thread_t *thread;
    ptr_t scheduler_stack;
    paging_handle_t pagetable;
} cpu_t;

typedef struct
{
    ptr_t vaddr; // virtual addresses
    size_t npages;
    vm_flags flags; // the expected flags for the region, regardless of the copy-on-write state
    paging_handle_t address_space;
} vmblock_t;

/**
 * @brief Information about a page table iteration
 */
typedef struct
{
    paging_handle_t address_space;
    ptr_t vaddr_start;
    size_t npages;
} pgt_iteration_info_t;

/**
 * @brief Callback for platform_mm_iterate_table
 *
 * @param vaddr_start The virtual address of the page
 * @param paddr_start The physical address of the page
 * @param n_pages The number of pages in the range
 * @param flags The flags of the page, (e.g. permissions)
 * @param arg The argument passed to platform_mm_iterate_phys_addr
 *
 * @note For saving CPU cycles, the callback should be called for each contiguous range of pages,
 * instead of each page individually, where the n_pages parameter indicates the number of pages
 * in the range.
 *
 */
typedef void (*pgt_iteration_callback_t)(const pgt_iteration_info_t *iter_info, const vmblock_t *block, ptr_t block_paddr, void *arg);

typedef struct
{
    u32 num_cpus;
    u32 boot_cpu_id;
    PER_CPU_DECLARE(cpu_t, cpu);

    vmblock_t k_code, k_rwdata, k_rodata;

    paging_handle_t kernel_pgd;
} mos_platform_info_t;

extern mos_platform_info_t *const platform_info;

extern void mos_start_kernel(void);
extern void mos_kernel_mm_init(void);

// Platform Machine APIs
noreturn void platform_shutdown(void);

// Platform CPU APIs
noreturn void platform_halt_cpu(void);
void platform_invalidate_tlb(void);
u32 platform_current_cpu_id(void);
void platform_msleep(u64 ms);
void platform_usleep(u64 us);
void platform_cpu_idle(void);

// Platform Interrupt APIs
void platform_interrupt_enable(void);
void platform_interrupt_disable(void);
bool platform_irq_handler_install(u32 irq, irq_handler handler);
void platform_irq_handler_remove(u32 irq, irq_handler handler);

// Platform Page Table APIs
paging_handle_t platform_mm_create_user_pgd(void);
void platform_mm_destroy_user_pgd(paging_handle_t table);

// Platform Paging APIs
void platform_mm_map_pages(paging_handle_t table, ptr_t vaddr, ptr_t paddr, size_t n_pages, vm_flags flags);
void platform_mm_unmap_pages(paging_handle_t table, ptr_t vaddr, size_t n_pages);
void platform_mm_iterate_table(paging_handle_t table, ptr_t vaddr, size_t n, pgt_iteration_callback_t callback, void *arg);
void platform_mm_flag_pages(paging_handle_t table, ptr_t vaddr, size_t n, vm_flags flags);
vm_flags platform_mm_get_flags(paging_handle_t table, ptr_t vaddr);
ptr_t platform_mm_get_phys_addr(paging_handle_t table, ptr_t vaddr);

// Platform Thread / Process APIs
void platform_context_setup(thread_t *thread, thread_entry_t entry, void *arg);
void platform_setup_forked_context(const thread_context_t *from, thread_context_t **to);

// Platform Context Switching APIs
void platform_switch_to_thread(ptr_t *old_stack, const thread_t *new_thread, switch_flags_t switch_flags);
void platform_switch_to_scheduler(ptr_t *old_stack, ptr_t new_stack);

// Platform-Specific syscall APIs
u64 platform_arch_syscall(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

// Platform-Specific IPI (Inter-Processor Interrupt) APIs
void platform_ipi_send(u8 target_cpu, ipi_type_t type);
