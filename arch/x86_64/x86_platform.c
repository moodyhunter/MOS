// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/console.h"
#include "mos/mm/mm.h"
#include "mos/tasks/schedule.h"
#include "mos/x86/acpi/acpi_types.h"
#include "mos/x86/cpu/ap_entry.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/descriptors/descriptors.h"
#include "mos/x86/devices/rtc.h"
#include "mos/x86/devices/serial.h"

#include <mos/cmdline.h>
#include <mos/kallsyms.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/setup.h>
#include <mos/x86/acpi/acpi.h>
#include <mos/x86/acpi/madt.h>
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
        pr_cont("%c", c);
        if (c == '\r')
            pr_cont("\n");
    }
}

static void x86_cpu_enable_sse(void)
{
    reg_t cr0 = x86_cpu_get_cr0();
    cr0 &= ~0x4; // clear coprocessor emulation CR0.EM
    cr0 |= 0x2;  // set coprocessor monitoring  CR0.MP
    x86_cpu_set_cr0(cr0);

    reg_t cr4 = x86_cpu_get_cr4();
    cr4 |= 0x3 << 9; // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    x86_cpu_set_cr4(cr4);
}

static void x86_cpu_enable_avx(void)
{
    // 0    X87 (x87 FPU/MMX State, note, must be '1')
    // 1    SSE (XSAVE feature set enable for MXCSR and XMM regs)
    // 2    AVX (AVX enable, and XSAVE feature set can be used to manage YMM regs)
    // 3    BNDREG (MPX enable, and XSAVE feature set can be used for BND regs)
    // 4    BNDCSR (MPX enable, and XSAVE feature set can be used for BNDCFGU and BNDSTATUS regs)
    // 5    opmask (AVX-512 enable, and XSAVE feature set can be used for AVX opmask, AKA k-mask, regs)
    // 6    ZMM_hi256 (AVX-512 enable, and XSAVE feature set can be used for upper-halves of the lower ZMM regs)
    // 7    Hi16_ZMM (AVX-512 enable, and XSAVE feature set can be used for the upper ZMM regs)
    // 8    PT (Processor Trace)
    // 9    PKRU (XSAVE feature set can be used for PKRU register, which is part of the protection keys mechanism)
    // 10   PASID
    // 11   CET_U
    // 12   CET_S
    // 13   HDC (Hardware duty cycling)
    // 14   UINTR (User interrupts)
    // 15   LBR (Last branch record)
    // 16   HWP (Hardware P-states)
    // 17   AMX TILECFG
    // 18   AMX TILEDATA
    // 19   APX extended GPRs (R16 through R31)

    __asm__ volatile("xor %%rcx, %%rcx;"
                     "xgetbv;"
                     "or $7, %%eax;" // x87, SSE, AVX
                     "xsetbv;"
                     :
                     :
                     : "rax", "rcx", "rdx");
}

static void x86_dump_stack_at(ptr_t this_frame)
{
    struct frame_t
    {
        struct frame_t *bp;
        ptr_t ip;
    } *frame = (struct frame_t *) this_frame;

    const bool do_mapped_check = current_cpu->mm_context;

    if (unlikely(!do_mapped_check))
        pr_warn("  no mm context available, mapping checks are disabled (early-boot panic?)");

    const bool no_relock = spinlock_is_locked(&current_cpu->mm_context->mm_lock);
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

        if (frame == frame->bp)
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
        else
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
                pr_warn(TRACE_FMT "<userspace, unknown>", i, frame->ip);
            }

            spinlock_release(&vmap->lock);

            if (!no_relock)
                spinlock_release(&current_cpu->mm_context->mm_lock);
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
    x86_dump_stack_at(frame);
}

void platform_dump_stack(platform_regs_t *regs)
{
    x86_dump_stack_at(regs->bp);
}

void platform_startup_early()
{
    x86_idt_init();
    x86_init_percpu_gdt();
    x86_init_percpu_idt();
    x86_init_percpu_tss();
    x86_init_irq_handlers();
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
}

void platform_startup_late()
{
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

    acpi_parse_rsdt(acpi_rsdp);

    mos_debug(x86_startup, "Initializing APICs...");
    madt_parse_table();
    lapic_memory_setup();
    lapic_enable(); // enable the local APIC
    pic_remap_irq();
    ioapic_init();

    rtc_init();

    x86_install_interrupt_handler(IRQ_CMOS_RTC, rtc_irq_handler);
    x86_install_interrupt_handler(IRQ_KEYBOARD, x86_keyboard_handler);
    x86_install_interrupt_handler(IRQ_COM1, x86_com1_handler);

    ioapic_enable_interrupt(IRQ_CMOS_RTC, x86_platform.boot_cpu_id);
    ioapic_enable_interrupt(IRQ_KEYBOARD, x86_platform.boot_cpu_id);
    ioapic_enable_interrupt(IRQ_COM1, x86_platform.boot_cpu_id);

    current_cpu->id = x86_platform.boot_cpu_id = lapic_get_id();

#if MOS_DEBUG_FEATURE(x86_startup)
    // begin detecting CPU features
    mos_debug(x86_startup, "cpu features:");
#define do_print_cpu_feature(feature)                                                                                                                                    \
    if (cpu_has_feature(CPU_FEATURE_##feature))                                                                                                                          \
        pr_cont(" " #feature);
    FOR_ALL_CPU_FEATURES(do_print_cpu_feature)
#undef do_print_cpu_feature
#endif

    x86_cpu_enable_sse();

    if (cpu_has_feature(CPU_FEATURE_AVX))
        x86_cpu_enable_avx();

#if MOS_CONFIG(MOS_SMP)
    x86_start_all_aps();
#endif
}
