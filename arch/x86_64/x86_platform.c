// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "mos/device/console.h"
#include "mos/mm/mm.h"
#include "mos/mm/paging/paging.h"
#include "mos/printk.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/acpi/acpi_types.h"
#include "mos/x86/acpi/madt.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/descriptors/descriptors.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/devices/rtc.h"
#include "mos/x86/devices/serial.h"
#include "mos/x86/devices/serial_console.h"
#include "mos/x86/interrupt/apic.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_interrupt.h"

#include <mos_stdlib.h>
#include <mos_string.h>

static u8 com1_buf[MOS_PAGE_SIZE] __aligned(MOS_PAGE_SIZE) = { 0 };
static u8 com2_buf[MOS_PAGE_SIZE] __aligned(MOS_PAGE_SIZE) = { 0 };

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
        .read.size = MOS_PAGE_SIZE,
        .default_fg = LightBlue,
        .default_bg = Black,
    },
};

serial_console_t com2_console = {
    .device = {
            .port = COM2,
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
        .name = "serial_com2",
        .caps = CONSOLE_CAP_EXTRA_SETUP,
        .read.buf = com2_buf,
        .read.size = MOS_PAGE_SIZE,
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
        if (c == '\r')
            c = '\n';
        serial_console_write(&com1_console.con, &c, 1);
        console_putc(&com1_console.con, c);
    }
}

typedef struct _frame
{
    struct _frame *bp;
    ptr_t ip;
} frame_t;

void x86_dump_stack_at(ptr_t this_frame, bool can_access_vmaps)
{
    frame_t *frame = (frame_t *) this_frame;

    const bool do_mapped_check = current_cpu->mm_context;

    if (unlikely(!do_mapped_check))
        pr_warn("  no mm context available, mapping checks are disabled (early-boot panic?)");

    const bool no_relock = do_mapped_check && spinlock_is_locked(&current_cpu->mm_context->mm_lock);
    if (no_relock)
        pr_emerg("  mm lock is already held, stack trace may be corrupted");

    pr_info("-- stack trace:");
    for (u32 i = 0; frame; i++)
    {
#define TRACE_FMT "  %-3d [" PTR_FMT "]: "
        if (do_mapped_check)
        {
            const pfn_t pfn = mm_get_phys_addr(current_cpu->mm_context, (ptr_t) frame) / MOS_PAGE_SIZE;
            if (!pfn)
            {
                pr_emerg(TRACE_FMT "<corrupted>, aborting backtrace", i, (ptr_t) frame);
                break;
            }
        }

        if (frame->bp == 0)
        {
            // end of stack
            pr_warn(TRACE_FMT "<end>", i, (ptr_t) 0);
            break;
        }
        else if (frame == frame->bp)
        {
            pr_emerg(TRACE_FMT "<corrupted>, aborting backtrace", i, (ptr_t) frame);
            break;
        }
        else if (frame->ip >= MOS_KERNEL_START_VADDR)
        {
            pr_warn(TRACE_FMT "%ps", i, frame->ip, (void *) frame->ip);
        }
        else if (frame->ip == 0)
        {
            pr_warn(TRACE_FMT "<end>", i, frame->ip);
            break;
        }
        else if (frame->ip < 1 KB)
        {
            pr_emerg(TRACE_FMT "<corrupted?>", i, frame->ip);
        }
        else if (can_access_vmaps)
        {
            if (!no_relock)
                spinlock_acquire(&current_cpu->mm_context->mm_lock);
            vmap_t *const vmap = vmap_obtain(current_cpu->mm_context, (ptr_t) frame->ip, NULL);

            if (vmap && vmap->io)
            {
                char filepath[MOS_PATH_MAX_LENGTH];
                io_get_name(vmap->io, filepath, sizeof(filepath));
                pr_warn(TRACE_FMT "%s (+" PTR_VLFMT ")", i, frame->ip, filepath, frame->ip - vmap->vaddr + vmap->io_offset);
            }
            else
            {
                pr_warn(TRACE_FMT "<userspace?, unknown>", i, frame->ip);
            }

            if (vmap)
                spinlock_release(&vmap->lock);

            if (!no_relock)
                spinlock_release(&current_cpu->mm_context->mm_lock);
        }
        else
        {
            pr_warn(TRACE_FMT "<unknown>", i, frame->ip);
        }
        frame = frame->bp;
    }
#undef TRACE_FMT
    pr_info("-- end of stack trace");
}

void platform_dump_current_stack(void)
{
    ptr_t frame;
    __asm__("mov %%rbp, %0" : "=r"(frame));
    x86_dump_stack_at(frame, true);
}

void platform_dump_stack(platform_regs_t *regs)
{
    x86_dump_stack_at(regs->bp, true);
}

void platform_startup_early()
{
    x86_idt_init();
    x86_init_irq_handlers();
    x86_init_percpu_gdt();
    x86_init_percpu_idt();
    x86_init_percpu_tss();

    x86_cpu_initialise_caps();
    x86_platform.arch_info.xsave_size = x86_cpu_setup_xsave_area();

#if MOS_DEBUG_FEATURE(x86_startup)
    pr_info2("cpu features:");

#define do_print_cpu_feature(feature)                                                                                                                                    \
    if (cpu_has_feature(CPU_FEATURE_##feature))                                                                                                                          \
        pr_cont(" " #feature);
    FOR_ALL_CPU_FEATURES(do_print_cpu_feature)

#undef do_print_cpu_feature

#endif
}

void platform_startup_mm()
{
    x86_initialise_phyframes_array();
    x86_paging_setup();

    // enable paging
    x86_cpu_set_cr3(pgd_pfn(x86_platform.kernel_mm->pgd) * MOS_PAGE_SIZE);

    pmm_reserve_frames(X86_BIOS_MEMREGION_PADDR / MOS_PAGE_SIZE, BIOS_MEMREGION_SIZE / MOS_PAGE_SIZE);
    pmm_reserve_frames(X86_EBDA_MEMREGION_PADDR / MOS_PAGE_SIZE, EBDA_MEMREGION_SIZE / MOS_PAGE_SIZE);
}

void platform_startup_late()
{
    pr_dinfo2(x86_startup, "Parsing ACPI tables...");

    if (platform_info->arch_info.rsdp_addr)
    {
        pr_dinfo2(x86_startup, "Using RSDP from bootloader: " PTR_FMT, platform_info->arch_info.rsdp_addr);
        acpi_rsdp = (const acpi_rsdp_t *) platform_info->arch_info.rsdp_addr;
    }
    else
    {
        pr_dinfo2(x86_startup, "Searching for RSDP in EBDA...");
        acpi_rsdp = acpi_find_rsdp(pa_va(X86_EBDA_MEMREGION_PADDR), EBDA_MEMREGION_SIZE);
        if (!acpi_rsdp)
        {
            pr_dinfo2(x86_startup, "Searching for RSDP in BIOS memory region...");
            acpi_rsdp = acpi_find_rsdp(pa_va(X86_BIOS_MEMREGION_PADDR), BIOS_MEMREGION_SIZE);
            if (!acpi_rsdp)
                mos_panic("RSDP not found");
        }
    }

    const pmm_region_t *acpi_region = pmm_find_reserved_region(acpi_rsdp->v1.rsdt_addr);
    MOS_ASSERT_X(acpi_region && acpi_region->reserved, "ACPI region not found or not reserved");

    acpi_parse_rsdt(acpi_rsdp);

    pr_dinfo2(x86_startup, "Initializing APICs...");
    madt_parse_table();
    lapic_enable(); // enable the local APIC
    current_cpu->id = x86_platform.boot_cpu_id = lapic_get_id();

    pic_remap_irq();
    ioapic_init();

    rtc_init();

    x86_install_interrupt_handler(IRQ_CMOS_RTC, rtc_irq_handler);
    x86_install_interrupt_handler(IRQ_KEYBOARD, x86_keyboard_handler);
    x86_install_interrupt_handler(IRQ_COM1, x86_com1_handler);

    ioapic_enable_interrupt(IRQ_CMOS_RTC, x86_platform.boot_cpu_id);
    ioapic_enable_interrupt(IRQ_KEYBOARD, x86_platform.boot_cpu_id);
    ioapic_enable_interrupt(IRQ_COM1, x86_platform.boot_cpu_id);

#if MOS_CONFIG(MOS_SMP)
    x86_start_all_aps();
#endif
}
