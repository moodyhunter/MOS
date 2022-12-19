// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/kconfig.h"
#include "mos/mos_global.h"
#include "mos/types.h"

#define PER_CPU_DECLARE(type, name)                                                                                                                                      \
    struct                                                                                                                                                               \
    {                                                                                                                                                                    \
        type percpu_value[MOS_MAX_CPU_COUNT];                                                                                                                            \
    } name

#define per_cpu(var) (&(var.percpu_value[platform_current_cpu_id()]))

#define current_cpu     per_cpu(platform_info->cpu)
#define current_thread  (current_cpu->thread)
#define current_process (current_thread->owner)

typedef void (*thread_entry_t)(void *arg);
typedef void (*irq_handler)(u32 irq);

typedef struct _downwards_stack_t downwards_stack_t;
typedef struct _thread thread_t;
typedef struct _page_map page_map_t;

typedef enum
{
    VM_NONE = 1 << 1,
    VM_READ = 1 << 2,
    VM_WRITE = 1 << 3,
    VM_USER = 1 << 4,
    VM_WRITE_THROUGH = 1 << 5,
    VM_CACHE_DISABLED = 1 << 6,
    VM_GLOBAL = 1 << 7,
    VM_EXEC = 1 << 8,

    // composite flags (for convenience)
    VM_RW = VM_READ | VM_WRITE,
    VM_USER_RW = VM_USER | VM_RW,
} vm_flags;

typedef enum
{
    PGALLOC_HINT_KHEAP,
    PGALLOC_HINT_UHEAP,
    PGALLOC_HINT_STACK,
    PGALLOC_HINT_MMAP,
} pgalloc_hints;

typedef enum
{
    VMTYPE_APPCODE,      // '.text' section
    VMTYPE_APPDATA,      // '.data' sections
    VMTYPE_APPDATA_ZERO, // '.bss' sections (zeroed out)
    VMTYPE_HEAP,         // userspace heap
    VMTYPE_STACK,        // userspace stack
    VMTYPE_KSTACK,       // kernel stack
    VMTYPE_SHM,          // shared memory
    VMTYPE_FILE,         // file mapping (mmap)
} vm_type;

typedef enum
{
    HEAP_GET_BASE,
    HEAP_GET_TOP,
    HEAP_SET_TOP,
    HEAP_GET_SIZE,
    HEAP_GROW_PAGES,
} heap_control_op;

typedef struct
{
    uintptr_t pgd;
    page_map_t *um_page_map;
} paging_handle_t;

typedef struct
{
    uintptr_t instruction;
    uintptr_t stack;
} __packed platform_context_t;

typedef struct
{
    u32 id;
    thread_t *thread;
    uintptr_t scheduler_stack;
    paging_handle_t pagetable;
} cpu_t;

typedef struct
{
    uintptr_t vaddr, paddr; // virtual and physical addresses
    size_t npages;
    vm_flags flags; // the expected flags for the region, regardless of the copy-on-write state
} vmblock_t;

typedef struct
{
    uintptr_t address;
    size_t size_bytes;
    bool available;
} memregion_t;

typedef struct
{
    u32 num_cpus;
    u32 boot_cpu_id;
    PER_CPU_DECLARE(cpu_t, cpu);

    vmblock_t k_code, k_rwdata, k_rodata;
    size_t num_mem_regions;
    memregion_t mem_regions[MOS_MAX_SUPPORTED_MEMREGION];
    size_t available_mem_bytes, total_mem_bytes;

    paging_handle_t kernel_pgd;
} mos_platform_info_t;

extern mos_platform_info_t *const platform_info;

extern void mos_start_kernel(const char *cmdline);
extern void mos_kernel_mm_init(void);

// Platform Machine APIs
noreturn void platform_shutdown(void);

// Platform CPU APIs
void platform_halt_cpu(void);
u32 platform_current_cpu_id(void);
void platform_msleep(u64 ms);
void platform_usleep(u64 us);

// Platform Interrupt APIs
void platform_interrupt_enable(void);
void platform_interrupt_disable(void);
bool platform_irq_handler_install(u32 irq, irq_handler handler);
void platform_irq_handler_remove(u32 irq, irq_handler handler);

// Platform Page Table APIs
paging_handle_t platform_mm_create_user_pgd(void);
void platform_mm_destroy_user_pgd(paging_handle_t table);

// Platform Paging APIs
void platform_mm_map_pages(paging_handle_t table, vmblock_t block);
void platform_mm_unmap_pages(paging_handle_t table, uintptr_t vaddr, size_t n_pages);
vmblock_t platform_mm_get_block_info(paging_handle_t table, uintptr_t vaddr, size_t npages);
vmblock_t platform_mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages);
void platform_mm_flag_pages(paging_handle_t table, uintptr_t vaddr, size_t n, vm_flags flags);
vm_flags platform_mm_get_flags(paging_handle_t table, uintptr_t vaddr);

// Platform Thread / Process APIs
void platform_context_setup(thread_t *thread, thread_entry_t entry, void *arg);
void platform_context_copy(platform_context_t *from, platform_context_t **to);

// Platform Context Switching APIs
void platform_switch_to_thread(uintptr_t *old_stack, thread_t *new_thread);
void platform_switch_to_scheduler(uintptr_t *old_stack, uintptr_t new_stack);
