// SPDX-License-Identifier: GPL-3.0-or-later

// #include "mos/device/console.hpp"
#include "mos/device/serial_console.hpp"
#include "mos/interrupt/interrupt.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/paging/pmlx/pml3.hpp"
#include "mos/mm/paging/pmlx/pml4.hpp"
#include "mos/platform/platform.hpp"
#include "mos/riscv64/cpu/cpu.hpp"
#include "mos/riscv64/cpu/plic.hpp"
#include "mos/riscv64/devices/uart_driver.hpp"

static u8 uart_buf[MOS_PAGE_SIZE] __aligned(MOS_PAGE_SIZE) = { 0 };

static bool riscv64_uart_setup(console_t *con)
{
    serial_console_t *const serial_con = container_of(con, serial_console_t, con);
    serial_con->device.driver_data = (void *) pa_va(0x10000000);
    serial_console_setup(con);
    return true;
}

static console_ops_t uart_console_ops = {
    .extra_setup = riscv64_uart_setup,
};

static serial_console_t uart_console = {
    .con = { .ops = &uart_console_ops,
             .name = "riscv_uart1",
             .caps = CONSOLE_CAP_READ | CONSOLE_CAP_EXTRA_SETUP,
             .read = { .buf = uart_buf, .size = MOS_PAGE_SIZE },
             .default_fg = LightBlue,
             .default_bg = Black },
    .device = { .driver = &riscv64_uart_driver,
                .driver_data = NULL,
                .baudrate_divisor = BAUD_RATE_115200,
                .char_length = CHAR_LENGTH_8,
                .stop_bits = STOP_BITS_1,
                .parity = PARITY_NONE },
};

static mos_platform_info_t riscv64_platform_info = {
    .boot_console = &uart_console.con,
};
mos_platform_info_t *const platform_info = &riscv64_platform_info;

void platform_startup_early()
{
    platform_info->num_cpus = 1;

    write_csr(stvec, (ptr_t) __riscv64_trap_entry);

    reg_t sstatus = read_csr(sstatus);
    sstatus |= SSTATUS_SUM;
    sstatus |= SSTATUS_FS_INITIAL; // enable sstatus.FS
    write_csr(sstatus, sstatus);
    write_csr(sie, SIE_SEIE | SIE_STIE | SIE_SSIE); // enable supervisor interrupts
}

void platform_startup_setup_kernel_mm()
{
    // even SV39 supports 1 GB pages
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
#define UART0_IRQ 10
    plic_enable_irq(UART0_IRQ);
    interrupt_handler_register(UART0_IRQ, serial_console_irq_handler, &uart_console.con);
}
