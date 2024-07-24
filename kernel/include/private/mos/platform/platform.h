// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/interrupt/ipi.h"
#include "mos/misc/cmdline.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform_defs.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/mm_types.h>
#include <mos/mos_global.h>
#include <mos/tasks/signal_types.h>

// clang-format off
#if MOS_CONFIG(MOS_SMP)
#define PER_CPU_DECLARE(type, name) struct name { type percpu_value[MOS_MAX_CPU_COUNT]; } name
#define PER_CPU_VAR_INIT { .percpu_value = { 0 } }
#define per_cpu(var) (&(var.percpu_value[platform_current_cpu_id()]))
#else
#define PER_CPU_DECLARE(type, name) struct name { type percpu_value[1]; } name
#define PER_CPU_VAR_INIT { .percpu_value = { 0 } }
#define per_cpu(var) (&(var.percpu_value[0]))
#endif
// clang-format on

#define current_cpu     per_cpu(platform_info->cpu)
#define current_thread  (current_cpu->thread)
#define current_process (current_thread->owner)
#define current_mm      (current_cpu->mm_context)

typedef void (*irq_handler)(u32 irq);

typedef struct _thread thread_t;
typedef struct _console console_t;

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
    VM_RX = VM_READ | VM_EXEC,
    VM_RWX = VM_READ | VM_WRITE | VM_EXEC,
    VM_USER_RW = VM_USER | VM_RW,
    VM_USER_RX = VM_USER | VM_RX,
    VM_USER_RO = VM_USER | VM_READ,
    VM_USER_RWX = VM_USER | VM_RWX,
} vm_flags;

typedef enum
{
    THREAD_STATE_CREATED,          ///< created or forked, but not ever started
    THREAD_STATE_READY,            ///< thread can be scheduled
    THREAD_STATE_RUNNING,          ///< thread is currently running
    THREAD_STATE_BLOCKED,          ///< thread is blocked by a wait condition
    THREAD_STATE_NONINTERRUPTIBLE, ///< thread is blocked, and cannot be interrupted
    THREAD_STATE_DEAD,             ///< thread is dead, and will be cleaned up soon by the scheduler
} thread_state_t;

typedef enum
{
    SWITCH_REGULAR,
    SWITCH_TO_NEW_USER_THREAD,
    SWITCH_TO_NEW_KERNEL_THREAD,
} switch_flags_t;

MOS_ENUM_OPERATORS(switch_flags_t)

typedef struct
{
    spinlock_t mm_lock; ///< protects [pgd] and the [mmaps] list (the list itself, not the vmap_t objects)
    pgd_t pgd;
    list_head mmaps;
} mm_context_t;

typedef struct _platform_regs platform_regs_t;

typedef struct
{
    u32 id;
    thread_t *thread;
    ptr_t scheduler_stack;
    mm_context_t *mm_context;
    platform_regs_t *interrupt_regs; ///< the registers of whatever interrupted this CPU
    platform_cpuinfo_t cpuinfo;
    thread_t *idle_thread; ///< idle thread for this CPU
} cpu_t;

typedef struct
{
    u8 second;
    u8 minute;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} timeval_t;

typedef struct
{
    u32 num_cpus;
    u32 boot_cpu_id;
    PER_CPU_DECLARE(cpu_t, cpu);

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

    platform_arch_info_t arch_info;

    console_t *boot_console;
} mos_platform_info_t;

#define MOS_KERNEL_PFN(vaddr) ((ALIGN_DOWN_TO_PAGE((vaddr) - (platform_info->k_basevaddr)) / MOS_PAGE_SIZE) + (platform_info->k_basepfn))

extern mos_platform_info_t *const platform_info;

typedef struct _platform_process_options platform_process_options_t;
typedef struct _platform_thread_options platform_thread_options_t;

__BEGIN_DECLS

// should be defined in platform's linker script
extern const char __MOS_KERNEL_CODE_START[], __MOS_KERNEL_CODE_END[];     // Kernel text
extern const char __MOS_KERNEL_RODATA_START[], __MOS_KERNEL_RODATA_END[]; // Kernel rodata
extern const char __MOS_KERNEL_RW_START[], __MOS_KERNEL_RW_END[];         // Kernel read-write data
extern const char __MOS_KERNEL_END[];                                     // Kernel end

extern void mos_start_kernel(void);

#define platform_alias(name) __attribute__((alias("platform_default_" #name)))

// Platform Startup APIs
void platform_ap_entry(u64 arg);
void platform_startup_early();
void platform_startup_setup_kernel_mm();
void platform_startup_late();

// Platform Machine APIs
// default implementation panics
[[noreturn]] void platform_shutdown(void);
// default implementations do nothing for 4 functions below
void platform_dump_regs(platform_regs_t *regs);
void platform_dump_stack(platform_regs_t *regs);
void platform_dump_current_stack();
void platform_dump_thread_kernel_stack(const thread_t *thread);

// Platform Timer/Clock APIs
// default implementation does nothing
void platform_get_time(timeval_t *val);

// Platform CPU APIs
// default implementation loops forever
[[noreturn]] void platform_halt_cpu(void);
// default implementation does nothing for 4 functions below
void platform_invalidate_tlb(ptr_t vaddr);
u32 platform_current_cpu_id(void);
void platform_cpu_idle(void);
u64 platform_get_timestamp(void);

typedef char datetime_str_t[32];
datetime_str_t *platform_get_datetime_str(void);

// Platform Interrupt APIs
// default implementation does nothing
void platform_interrupt_enable(void);
void platform_interrupt_disable(void);

// Platform Page Table APIs
// no default implementation, platform-specific implementations must be provided
pfn_t platform_pml1e_get_pfn(const pml1e_t *pml1);            // returns the physical address contained in the pmlx entry,
void platform_pml1e_set_pfn(pml1e_t *pml1, pfn_t pfn);        // -- which can be a pfn for either a page or another page table
bool platform_pml1e_get_present(const pml1e_t *pml1);         // returns if an entry in this page table is present
void platform_pml1e_set_flags(pml1e_t *pml1, vm_flags flags); // set bits in the flags field of the pmlx entry
vm_flags platform_pml1e_get_flags(const pml1e_t *pml1e);      // get bits in the flags field of the pmlx entry

#if MOS_PLATFORM_PAGING_LEVELS >= 2
pml1_t platform_pml2e_get_pml1(const pml2e_t *pml2);
void platform_pml2e_set_pml1(pml2e_t *pml2, pml1_t pml1, pfn_t pml1_pfn);
bool platform_pml2e_get_present(const pml2e_t *pml2);
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
void platform_pml4e_set_flags(pml4e_t *pml4, vm_flags flags);
vm_flags platform_pml4e_get_flags(const pml4e_t *pml4e);
#if MOS_CONFIG(PML4_HUGE_CAPABLE)
bool platform_pml4e_is_huge(const pml4e_t *pml4);
void platform_pml4e_set_huge(pml4e_t *pml4, pfn_t pfn);
pfn_t platform_pml4e_get_huge_pfn(const pml4e_t *pml4);
#endif
#endif

// Platform Thread / Process APIs
// no default implementation, platform-specific implementations must be provided
platform_regs_t *platform_thread_regs(const thread_t *thread);
void platform_context_setup_main_thread(thread_t *thread, ptr_t entry, ptr_t sp, int argc, ptr_t argv, ptr_t envp);
void platform_context_setup_child_thread(thread_t *thread, thread_entry_t entry, void *arg);
void platform_context_clone(const thread_t *from, thread_t *to);
void platform_context_cleanup(thread_t *thread);

// Platform Context Switching APIs
// no default implementation, platform-specific implementations must be provided
void platform_switch_mm(const mm_context_t *new_mm);
void platform_switch_to_thread(thread_t *current, thread_t *new_thread, switch_flags_t switch_flags);
[[noreturn]] void platform_return_to_userspace(platform_regs_t *regs);

// Platform-Specific syscall APIs
// default implementation does nothing
u64 platform_arch_syscall(u64 syscall, u64 arg1, u64 arg2, u64 arg3, u64 arg4);

// Platform-Specific IPI (Inter-Processor Interrupt) APIs
// default implementation does nothing
void platform_ipi_send(u8 target_cpu, ipi_type_t type);

// Signal Handler APIs
// the 4 function below has default implementations that panic if not implemented
typedef struct _sigreturn_data sigreturn_data_t;
[[noreturn]] void platform_jump_to_signal_handler(const platform_regs_t *regs, const sigreturn_data_t *sigreturn_data, const sigaction_t *sa);
[[noreturn]] void platform_restore_from_signal_handler(void *sp);
void platform_syscall_setup_restart_context(platform_regs_t *regs, reg_t syscall_nr);
void platform_syscall_store_retval(platform_regs_t *regs, reg_t result);

__END_DECLS
