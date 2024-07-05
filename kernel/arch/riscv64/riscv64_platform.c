// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/panic.h"
#include "mos/mm/mm.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/platform/platform.h"
#include "mos/riscv64/cpu/cpu.h"
#include "mos/riscv64/devices/sbi_console.h"

#include <mos_stdlib.h>

#define SSTATUS_SPP BIT(8)
#define SSTATUS_SUM BIT(18)

static mos_platform_info_t riscv64_platform_info;
mos_platform_info_t *const platform_info = &riscv64_platform_info;

static mos_platform_info_t riscv64_platform_info = {
    .boot_console = &sbi_console,
};

extern void riscv64_trap_exit(platform_regs_t *regs);
extern void riscv64_trap_entry();

void riscv64_trap_handler(platform_regs_t *regs, reg_t scause, reg_t stval, reg_t sepc)
{
    current_cpu->interrupt_regs = regs;
    const reg_t sstatus = read_csr(sstatus);
    const bool userspace = (sstatus & SSTATUS_SPP) == 0;
    const bool interrupt = scause & BIT(63);
    const long exception_code = scause & ~BIT(63);

    const char *name = "<unknown>";
    if (interrupt)
    {
        switch (exception_code)
        {
            case 1: name = "Supervisor software interrupt"; break;
            case 5: name = "Supervisor timer interrupt"; break;
            case 9: name = "Supervisor external interrupt"; break;
            case 13: name = "Counter-overflow interrupt"; break;
            default: name = exception_code >= 16 ? "Designated for platform use" : "<reserved>";
        }
    }
    else
    {
        switch (exception_code)
        {
            case 0: name = "Instruction address misaligned"; break;
            case 1: name = "Instruction access fault"; break;
            case 2: name = "Illegal instruction"; goto handle_ii;
            case 3: name = "Breakpoint"; break;
            case 4: name = "Load address misaligned"; break;
            case 5: name = "Load access fault"; break;
            case 6: name = "Store/AMO address misaligned"; break;
            case 7: name = "Store/AMO access fault"; break;
            case 8: name = "Environment call from U-mode"; break;
            case 9: name = "Environment call from S-mode"; break;
            case 12: name = "Instruction page fault"; goto handle_pf;
            case 13: name = "Load page fault"; goto handle_pf;
            case 15: name = "Store/AMO page fault"; goto handle_pf;
            case 18: name = "Software check"; break;
            case 19: name = "Hardware error"; break;
            default:
                if (exception_code >= 24 && exception_code <= 31)
                    name = "Designated for custom use";
                else if (exception_code >= 48 && exception_code <= 63)
                    name = "Designated for custom use";
        }
    }

    pr_info("riscv64_trap_entry: sepc=" PTR_FMT ", scause" PTR_FMT ", stval=" PTR_FMT, sepc, scause, stval);
    pr_info("Exception: %s (code=%ld, interrupt=%d)", name, exception_code, interrupt);
    MOS_ASSERT(!"Unhandled exception");

handle_ii:;
    try_handle_kernel_panics(sepc);
    goto leave;

handle_pf:;
    MOS_ASSERT_X(!interrupt, "Page faults should not be interrupts");

    const bool present = mm_do_get_present(current_cpu->mm_context->pgd, stval);

    pagefault_t pfinfo = {
        .ip = sepc,
        .is_exec = exception_code == 12,
        .is_write = exception_code == 15,
        .is_present = present,
        .is_user = userspace,
    };
    mm_handle_fault(stval, &pfinfo);
    goto leave;

leave:
    riscv64_trap_exit(regs);
}

void platform_startup_early()
{
    platform_info->num_cpus = 1;
    write_csr(stvec, (ptr_t) riscv64_trap_entry);

    reg_t sstatus = read_csr(sstatus);
    sstatus |= SSTATUS_SUM;
    write_csr(sstatus, sstatus);
}

void platform_startup_setup_kernel_mm()
{
    // SV39 supports 1 GB pages
    const size_t STEP = 1 GB / MOS_PAGE_SIZE;
    const size_t total_npages = ALIGN_UP(platform_info->max_pfn, STEP);

    for (pfn_t pfn = 0; pfn < total_npages; pfn += STEP)
    {
        const ptr_t vaddr = pfn_va(pfn);
        pml4e_t *pml4e = pml4_entry(platform_info->kernel_mm->pgd.max.next, vaddr);

        // GB pages are at pml3e level
        const pml3_t pml3 = pml4e_get_or_create_pml3(pml4e);
        pml3e_t *const pml3e = pml3_entry(pml3, vaddr);
        platform_pml3e_set_huge(pml3e, pfn);
        platform_pml3e_set_flags(pml3e, VM_READ | VM_WRITE | VM_GLOBAL);
    }
}

void platform_startup_late()
{
}
