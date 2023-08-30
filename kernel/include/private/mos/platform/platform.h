// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/cmdline.h"
#include "mos/mm/paging/pml_types.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/mm_types.h>
#include <mos/tasks/signal_types.h>

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
#define current_mm      (current_cpu->mm_context)

typedef void (*irq_handler)(u32 irq);

typedef struct _thread thread_t;

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
    THREAD_STATE_CREATED, // created or forked, but not ever started
    THREAD_STATE_READY,   // thread can be scheduled
    THREAD_STATE_RUNNING, // thread is currently running
    THREAD_STATE_BLOCKED, // thread is blocked by a wait condition
    THREAD_STATE_DEAD,    // thread is dead, and will be cleaned up soon by the scheduler
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

typedef struct _io io_t;

/**
 * @brief A wrapper type for the standard I/O streams
 */
typedef struct
{
    io_t *in, *out, *err;
} stdio_t;

typedef struct
{
    spinlock_t mm_lock; ///< protects [pgd] and the [mmaps] list (the list itself, not the vmap_t objects)
    pgd_t pgd;
    list_head mmaps;
} mm_context_t;

typedef struct
{
    u32 id;
    thread_t *thread;
    ptr_t scheduler_stack;
    mm_context_t *mm_context;
} cpu_t;

typedef struct
{
    ptr_t vaddr; // virtual addresses
    size_t npages;
    vm_flags flags; // the expected flags for the region
} vmblock_t;

typedef struct
{
    u32 num_cpus;
    u32 boot_cpu_id;
    PER_CPU_DECLARE(cpu_t, cpu);

    vmblock_t k_code, k_rwdata, k_rodata;
    pfn_t k_basepfn;
    ptr_t k_basevaddr; // virtual address of the kernel base (i.e. the start of the kernel image)

    mm_context_t *kernel_mm;

    pfn_t initrd_pfn;
    size_t initrd_npages;

    pfn_t max_pfn;
    pmm_region_t pmm_regions[MOS_MAX_MEMREGIONS];
    size_t num_pmm_regions;

    ptr_t direct_map_base; // direct mapping to all physical memory

    size_t n_cmdlines;
    cmdline_option_t cmdlines[MOS_MAX_CMDLINE_COUNT];
} mos_platform_info_t;

extern mos_platform_info_t *const platform_info;

extern void mos_start_kernel(void);

// Platform Startup APIs
void platform_startup_early();
void platform_startup_mm();
void platform_startup_late();

// Platform Machine APIs
noreturn void platform_shutdown(void);

// Platform CPU APIs
noreturn void platform_halt_cpu(void);
void platform_invalidate_tlb(ptr_t vaddr);
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
pfn_t platform_pml1e_get_pfn(const pml1e_t *pml1);            // returns the physical address contained in the pmlx entry,
void platform_pml1e_set_pfn(pml1e_t *pml1, pfn_t pfn);        // -- which can be a pfn for either a page or another page table
bool platform_pml1e_get_present(const pml1e_t *pml1);         // returns if an entry in this page table is present
void platform_pml1e_set_present(pml1e_t *pml1, bool present); // sets if an entry in this page table is present
void platform_pml1e_set_flags(pml1e_t *pml1, vm_flags flags); // set bits in the flags field of the pmlx entry
vm_flags platform_pml1e_get_flags(const pml1e_t *pml1e);      // get bits in the flags field of the pmlx entry

#if MOS_PLATFORM_PAGING_LEVELS >= 2
pml1_t platform_pml2e_get_pml1(const pml2e_t *pml2);
void platform_pml2e_set_pml1(pml2e_t *pml2, pml1_t pml1, pfn_t pml1_pfn);
bool platform_pml2e_get_present(const pml2e_t *pml2);
void platform_pml2e_set_present(pml2e_t *pml2, bool present);
void platform_pml2e_set_flags(pml2e_t *pml2, vm_flags flags);
vm_flags platform_pml2e_get_flags(const pml2e_t *pml2e);
#if MOS_CONFIG(PML2_HUGE_CAPABLE)
bool platform_pml2e_is_huge(const pml2e_t *pml2);
void platform_pml2e_set_huge(pml2e_t *pml2, pfn_t pfn);
pfn_t platform_pml2e_get_huge_pfn(const pml2e_t *pml2);
#endif
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 3
pml2_t platform_pml3e_get_pml2(const pml3e_t *pml3);
void platform_pml3e_set_pml2(pml3e_t *pml3, pml2_t pml2, pfn_t pml2_pfn);
bool platform_pml3e_get_present(const pml3e_t *pml3);
void platform_pml3e_set_present(pml3e_t *pml3, bool present);
void platform_pml3e_set_flags(pml3e_t *pml3, vm_flags flags);
vm_flags platform_pml3e_get_flags(const pml3e_t *pml3e);
#if MOS_CONFIG(PML3_HUGE_CAPABLE)
bool platform_pml3e_is_huge(const pml3e_t *pml3);
void platform_pml3e_set_huge(pml3e_t *pml3, pfn_t pfn);
pfn_t platform_pml3e_get_huge_pfn(const pml3e_t *pml3);
#endif
#endif

#if MOS_PLATFORM_PAGING_LEVELS >= 4
pml3_t platform_pml4e_get_pml3(const pml4e_t *pml4);
void platform_pml4e_set_pml3(pml4e_t *pml4, pml3_t pml3, pfn_t pml3_pfn);
bool platform_pml4e_get_present(const pml4e_t *pml4);
void platform_pml4e_set_present(pml4e_t *pml4, bool present);
void platform_pml4e_set_flags(pml4e_t *pml4, vm_flags flags);
vm_flags platform_pml4e_get_flags(const pml4e_t *pml4e);
#if MOS_CONFIG(PML4_HUGE_CAPABLE)
bool platform_pml4e_is_huge(const pml4e_t *pml4);
void platform_pml4e_set_huge(pml4e_t *pml4, pfn_t pfn);
pfn_t platform_pml4e_get_huge_pfn(const pml4e_t *pml4);
#endif
#endif

// Platform Thread / Process APIs
void platform_context_setup(thread_t *thread, thread_entry_t entry, void *arg);
void platform_setup_forked_context(const void *from, void **to);

// Platform Context Switching APIs
void platform_switch_mm(mm_context_t *new_mm);
void platform_switch_to_thread(ptr_t *old_stack, const thread_t *new_thread, switch_flags_t switch_flags);
void platform_switch_to_scheduler(ptr_t *old_stack, ptr_t new_stack);

// Platform-Specific syscall APIs
u64 platform_arch_syscall(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

// Platform-Specific IPI (Inter-Processor Interrupt) APIs
void platform_ipi_send(u8 target_cpu, ipi_type_t type);

// Signal Handler APIs
void platform_jump_to_signal_handler(signal_t sig, sigaction_t *sa);
noreturn void platform_restore_from_signal_handler(void *sp);
