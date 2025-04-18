// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.hpp"

#include "mos/device/console.hpp"
#include "mos/device/serial.hpp"
#include "mos/device/serial_console.hpp"
#include "mos/interrupt/interrupt.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/paging/paging.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/schedule.hpp"
#include "mos/x86/acpi/acpi.hpp"
#include "mos/x86/acpi/acpi_types.hpp"
#include "mos/x86/acpi/madt.hpp"
#include "mos/x86/cpu/ap_entry.hpp"
#include "mos/x86/cpu/cpu.hpp"
#include "mos/x86/descriptors/descriptors.hpp"
#include "mos/x86/devices/port.hpp"
#include "mos/x86/devices/rtc.hpp"
#include "mos/x86/interrupt/apic.hpp"
#include "mos/x86/mm/paging_impl.hpp"
#include "mos/x86/x86_interrupt.hpp"

#include <ansi_colors.h>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

typedef enum : u16
{
    COM1 = 0x3F8,
    COM2 = 0x2F8,
    COM3 = 0x3E8,
    COM4 = 0x2E8,
    COM5 = 0x5F8,
    COM6 = 0x4F8,
    COM7 = 0x5E8,
    COM8 = 0x4E8
} x86ComPort;

class x86SerialDevice : public ISerialDevice
{
    x86ComPort port;

  public:
    explicit x86SerialDevice(x86ComPort port) : port(port)
    {
        baudrate_divisor = BAUD_RATE_115200;
        char_length = CHAR_LENGTH_8;
        stop_bits = STOP_BITS_15_OR_2;
        parity = PARITY_EVEN;
    }

  public:
    u8 ReadByte() override
    {
        return port_inb(port);
    }

    int WriteByte(u8 data) override
    {
        port_outb(port, data);
        return 0;
    }

    u8 read_register(serial_register_t reg) override
    {
        return port_inb((u16) port + reg);
    }

    void write_register(serial_register_t reg, u8 data) override
    {
        port_outb((u16) port + reg, data);
    }

    bool get_data_ready() override
    {
        return (port_inb(port + 5) & 1) != 0;
    }
};

static Buffer<MOS_PAGE_SIZE> com1_buf;
static Buffer<MOS_PAGE_SIZE> com2_buf;

x86SerialDevice com1_device{ COM1 };
x86SerialDevice com2_device{ COM2 };

SerialConsole COM1Console{ "com1_console", CONSOLE_CAP_READ, &com1_buf, &com1_device, LightBlue, Black };
SerialConsole COM2Console{ "com2_console", CONSOLE_CAP_READ, &com2_buf, &com2_device, LightBlue, Black };

mos_platform_info_t *const platform_info = &x86_platform;
mos_platform_info_t x86_platform = { .boot_console = &COM1Console };
const acpi_rsdp_t *acpi_rsdp = NULL;

static bool x86_keyboard_handler(u32 irq, void *data)
{
    MOS_UNUSED(data);
    MOS_ASSERT(irq == IRQ_KEYBOARD);
    int scancode = port_inb(0x60);

    pr_info("Keyboard scancode: %x", scancode);
    return true;
}

static bool x86_pit_timer_handler(u32 irq, void *data)
{
    MOS_UNUSED(data);
    MOS_ASSERT(irq == IRQ_PIT_TIMER);
    spinlock_acquire(&current_thread->state_lock);
    reschedule();
    return true;
}

void x86_setup_lapic_timer()
{
    lapic_set_timer(1000000);
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
            vmap_t *const vmap = vmap_obtain(current_cpu->mm_context, (ptr_t) frame->ip);

            if (vmap && vmap->io)
            {
                const auto name = vmap->io->name();
                pr_warn(TRACE_FMT "%s (+" PTR_VLFMT ")", i, frame->ip, name.c_str(), frame->ip - vmap->vaddr + vmap->io_offset);
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

void platform_dump_stack(const platform_regs_t *regs)
{
    x86_dump_stack_at(regs->bp, true);
}

void platform_startup_early()
{
    COM2Console.Register();
    x86_idt_init();
    x86_init_percpu_gdt();
    x86_init_percpu_idt();
    x86_init_percpu_tss();

    // happens before setting up the kernel MM
    x86_cpu_initialise_caps();

#if MOS_DEBUG_FEATURE(x86_startup)
    pr_info2("cpu features:");

#define do_print_cpu_feature(feature)                                                                                                                                    \
    if (cpu_has_feature(CPU_FEATURE_##feature))                                                                                                                          \
        pr_cont(" " #feature);
    FOR_ALL_CPU_FEATURES(do_print_cpu_feature)

#undef do_print_cpu_feature

#endif

    x86_cpu_setup_xsave_area();
}

void platform_startup_setup_kernel_mm()
{
    x86_paging_setup();
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

    interrupt_handler_register(IRQ_PIT_TIMER, x86_pit_timer_handler, NULL);
    interrupt_handler_register(IRQ_CMOS_RTC, rtc_irq_handler, NULL);
    interrupt_handler_register(IRQ_KEYBOARD, x86_keyboard_handler, NULL);
    interrupt_handler_register(IRQ_COM1, serial_console_irq_handler, &COM1Console);

    ioapic_enable_interrupt(IRQ_CMOS_RTC, x86_platform.boot_cpu_id);
    ioapic_enable_interrupt(IRQ_KEYBOARD, x86_platform.boot_cpu_id);
    ioapic_enable_interrupt(IRQ_COM1, x86_platform.boot_cpu_id);

    x86_setup_lapic_timer();

    x86_unblock_aps();
}
