// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/console.h"
#include "mos/x86/acpi/acpi_types.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/descriptors/descriptors.h"
#include "mos/x86/devices/serial.h"

#include <mos/cmdline.h>
#include <mos/kallsyms.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/setup.h>
#include <mos/x86/acpi/acpi.h>
#include <mos/x86/acpi/madt.h>
#include <mos/x86/cpu/smp.h>
#include <mos/x86/devices/port.h>
#include <mos/x86/devices/serial_console.h>
#include <mos/x86/interrupt/apic.h>
#include <mos/x86/mm/mm.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/tasks/context.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>
#include <mos_stdlib.h>
#include <mos_string.h>

static u8 com1_buf[MOS_PAGE_SIZE] __aligned(MOS_PAGE_SIZE) = { 0 };

serial_console_t com1_console = {
    .device = {
            .port = COM1,
            .baud_rate = BAUD_RATE_115200,
            .char_length = CHAR_LENGTH_8,
            .stop_bits = STOP_BITS_15_OR_2,
            .parity = PARITY_EVEN,
        },
    .con = {
        .ops =
            &(console_ops_t){
                .extra_setup = serial_console_setup,
            },
        .name = "serial_com1",
        .caps = CONSOLE_CAP_EXTRA_SETUP,
        .read.buf = com1_buf,
        .default_fg = LightBlue,
        .default_bg = Black,
    },
};

mos_platform_info_t *const platform_info = &x86_platform;
mos_platform_info_t x86_platform = { 0 };
const acpi_rsdp_t *acpi_rsdp = NULL;

static void x86_keyboard_handler(u32 irq)
{
    MOS_ASSERT(irq == IRQ_KEYBOARD);
    int scancode = port_inb(0x60);

    pr_info("Keyboard scancode: %x", scancode);
}

static void x86_com1_handler(u32 irq)
{
    MOS_ASSERT(irq == IRQ_COM1);
    while (serial_dev_get_data_ready(&com1_console.device))
    {
        char c = '\0';
        serial_device_read(&com1_console.device, &c, 1);
        console_putc(&com1_console.con, c);
    }
}

static void x86_do_backtrace(void)
{
    static bool is_tracing = false;
    if (is_tracing)
        return;
    pr_info("Stack trace:");
    struct frame_t
    {
        struct frame_t *ebp;
        ptr_t eip;
    } *frame = NULL;

    __asm__("movq %%rbp,%1" : "=r"(frame) : "r"(frame));

    const bool do_mapped_check = current_cpu->mm_context;

    if (unlikely(!do_mapped_check))
        pr_warn("  no mm context available, mapping checks are disabled (early-boot panic?)");

    for (u32 i = 0; frame; i++)
    {
#define TRACE_FMT "  %-3d [" PTR_FMT "]: "
        if (do_mapped_check)
        {
            const pfn_t pfn = mm_get_phys_addr(current_cpu->mm_context, (ptr_t) frame) / MOS_PAGE_SIZE;
            if (!pfn)
            {
                pr_warn(TRACE_FMT "<corrupted>, aborting backtrace", i, (ptr_t) frame);
                break;
            }
        }

        if (frame->eip >= MOS_KERNEL_START_VADDR)
            pr_warn(TRACE_FMT "%ps", i, frame->eip, (void *) frame->eip);
        else
            pr_warn(TRACE_FMT "<userspace?>", i, frame->eip);

        frame = frame->ebp;
    }
}

void platform_startup_early()
{
    panic_hook_declare(x86_do_backtrace, "Backtrace");
    panic_hook_install(&x86_do_backtrace_holder);

    x86_init_current_cpu_gdt();
    x86_idt_init();
    x86_init_current_cpu_tss();
    x86_irq_handler_init();

#if MOS_CONFIG(MOS_SMP)
    mos_debug(x86_startup, "copying memory for SMP boot...");
    x86_smp_copy_trampoline();
#endif
}

void platform_startup_mm()
{
    x86_initialise_phyframes_array();
    x86_paging_setup();

    // enable paging
    x86_cpu_set_cr3(pgd_pfn(x86_platform.kernel_mm->pgd) * MOS_PAGE_SIZE);
    __asm__ volatile("mov %%cr4, %%rax; orq $0x80, %%rax; mov %%rax, %%cr4" ::: "rax"); // and enable PGE

    pmm_reserve_frames(X86_BIOS_MEMREGION_PADDR / MOS_PAGE_SIZE, BIOS_MEMREGION_SIZE / MOS_PAGE_SIZE);
    pmm_reserve_frames(X86_EBDA_MEMREGION_PADDR / MOS_PAGE_SIZE, EBDA_MEMREGION_SIZE / MOS_PAGE_SIZE);

    if (platform_info->initrd_npages)
        pmm_reserve_frames(platform_info->initrd_pfn, platform_info->initrd_npages);

    mos_debug(x86_startup, "Parsing ACPI tables...");
    acpi_rsdp = acpi_find_rsdp(pa_va(X86_EBDA_MEMREGION_PADDR), EBDA_MEMREGION_SIZE);
    if (!acpi_rsdp)
    {
        acpi_rsdp = acpi_find_rsdp(pa_va(X86_BIOS_MEMREGION_PADDR), BIOS_MEMREGION_SIZE);
        if (!acpi_rsdp)
            mos_panic("RSDP not found");
    }

    const pmm_region_t *acpi_region = pmm_find_reserved_region(acpi_rsdp->v1.rsdt_addr);
    MOS_ASSERT_X(acpi_region && acpi_region->reserved, "ACPI region not found or not reserved");
}

void platform_startup_late()
{
    acpi_parse_rsdt(acpi_rsdp);

    mos_debug(x86_startup, "Initializing APICs...");
    madt_parse_table();
    lapic_memory_setup();
    lapic_enable();
    pic_remap_irq();
    ioapic_init();

    x86_install_interrupt_handler(IRQ_TIMER, x86_timer_handler);
    x86_install_interrupt_handler(IRQ_KEYBOARD, x86_keyboard_handler);
    x86_install_interrupt_handler(IRQ_COM1, x86_com1_handler);

    ioapic_enable_interrupt(IRQ_TIMER, x86_platform.boot_cpu_id);
    ioapic_enable_interrupt(IRQ_KEYBOARD, x86_platform.boot_cpu_id);
    ioapic_enable_interrupt(IRQ_COM1, x86_platform.boot_cpu_id);

    current_cpu->id = x86_platform.boot_cpu_id = lapic_get_id();

#if MOS_CONFIG(MOS_SMP)
    x86_smp_start_all();
#endif
}
