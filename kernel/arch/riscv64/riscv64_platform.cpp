// SPDX-License-Identifier: GPL-3.0-or-later

// #include "mos/device/console.hpp"
#include "mos/device/clocksource.hpp"
#include "mos/device/serial.hpp"
#include "mos/device/serial_console.hpp"
#include "mos/interrupt/interrupt.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/paging/pmlx/pml3.hpp"
#include "mos/mm/paging/pmlx/pml4.hpp"
#include "mos/platform/platform.hpp"
#include "mos/riscv64/cpu/cpu.hpp"
#include "mos/riscv64/cpu/plic.hpp"

static Buffer<MOS_PAGE_SIZE> uart_buf;

class RiscV64UartDevice : public ISerialDevice
{
  public:
    RiscV64UartDevice(void *mmio) : mmio(static_cast<volatile u8 *>(mmio))
    {
        baudrate_divisor = BAUD_RATE_115200;
        char_length = CHAR_LENGTH_8;
        stop_bits = STOP_BITS_1;
        parity = PARITY_NONE;
    }

  public:
    u8 ReadByte() override
    {
        return mmio[0];
    }

    int WriteByte(u8 data) override
    {
        mmio[0] = data;
        return 0;
    }

    u8 read_register(serial_register_t reg) override
    {
        return mmio[reg];
    }

    void write_register(serial_register_t reg, u8 data) override
    {
        mmio[reg] = data;
    }

    bool get_data_ready() override
    {
        return true;
    }

  private:
    volatile u8 *mmio;
};

static RiscV64UartDevice uart_serial_device{ (void *) pa_va(0x10000000) };

SerialConsole uart_console{ "riscv_uart1", CONSOLE_CAP_READ, &uart_buf, &uart_serial_device, LightBlue, Black };

static mos_platform_info_t riscv64_platform_info = {
    .boot_console = &uart_console,
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

clocksource_t goldfish{
    .name = "goldfish",
    .ticks = 0,
    .frequency = 500,
};

void platform_startup_late()
{
#define UART0_IRQ 10
    plic_enable_irq(UART0_IRQ);
    interrupt_handler_register(UART0_IRQ, serial_console_irq_handler, &uart_console);

    clocksource_register(&goldfish);
}
