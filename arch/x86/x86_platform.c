// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/kallsyms.h>
#include <mos/mm/kmalloc.h>
#include <mos/mm/paging/page_ops.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/mos_global.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/x86/acpi/acpi.h>
#include <mos/x86/acpi/madt.h>
#include <mos/x86/cpu/cpu.h>
#include <mos/x86/cpu/cpuid.h>
#include <mos/x86/cpu/smp.h>
#include <mos/x86/descriptors/descriptor_types.h>
#include <mos/x86/devices/initrd_blockdev.h>
#include <mos/x86/devices/port.h>
#include <mos/x86/devices/serial_console.h>
#include <mos/x86/interrupt/apic.h>
#include <mos/x86/interrupt/idt.h>
#include <mos/x86/mm/mm.h>
#include <mos/x86/mm/paging.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/tasks/context.h>
#include <mos/x86/x86_interrupt.h>
#include <mos/x86/x86_platform.h>
#include <string.h>

static size_t initrd_size = 0;
static ptr_t initrd_paddr = 0;

static char mos_cmdline[512];
static serial_console_t com1_console = {
    .device.port = COM1,
    .device.baud_rate = BAUD_RATE_115200,
    .device.char_length = CHAR_LENGTH_8,
    .device.stop_bits = STOP_BITS_1,
    .device.parity = PARITY_EVEN,
    .console.name = "serial_com1",
    .console.caps = CONSOLE_CAP_SETUP | CONSOLE_CAP_COLOR,
    .console.setup = serial_console_setup,
};

static vmblock_t x86_bios_block = {
    .npages = BIOS_MEMREGION_SIZE / MOS_PAGE_SIZE,
    .vaddr = BIOS_VADDR(X86_BIOS_MEMREGION_PADDR),
    .flags = VM_READ | VM_GLOBAL | VM_CACHE_DISABLED,
};

static vmblock_t x86_ebda_block = {
    .npages = EBDA_MEMREGION_SIZE / MOS_PAGE_SIZE,
    .vaddr = BIOS_VADDR(X86_EBDA_MEMREGION_PADDR),
    .flags = VM_READ | VM_GLOBAL | VM_CACHE_DISABLED,
};

mos_platform_info_t *const platform_info = &x86_platform;
mos_platform_info_t x86_platform = {
    .k_code = { .vaddr = (ptr_t) &__MOS_KERNEL_CODE_START, .flags = VM_READ | VM_EXEC | VM_GLOBAL },
    .k_rodata = { .vaddr = (ptr_t) &__MOS_KERNEL_RODATA_START, .flags = VM_READ | VM_GLOBAL },
    .k_rwdata = { .vaddr = (ptr_t) &__MOS_KERNEL_RW_START, .flags = VM_READ | VM_WRITE | VM_GLOBAL },
};

void x86_keyboard_handler(u32 irq)
{
    MOS_ASSERT(irq == IRQ_KEYBOARD);
    int scancode = port_inb(0x60);
    MOS_UNUSED(scancode);
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

    __asm__("movl %%ebp,%1" : "=r"(frame) : "r"(frame));
    for (u32 i = 0; frame; i++)
    {
        if (frame->eip >= MOS_KERNEL_START_VADDR)
        {
            const kallsyms_t *kallsyms = kallsyms_get_symbol(frame->eip);
            if (kallsyms)
                pr_warn("  %-2d" PTR_FMT ": %s (+" PTR_VLFMT ")", i, frame->eip, kallsyms->name, frame->eip - kallsyms->address);
            else
                pr_warn("  %-2d" PTR_FMT ": <unknown>", i, frame->eip);
        }
        else
        {
            pr_warn("  %-2d" PTR_FMT ": <userspace?>", i, frame->eip);
        }

        frame = frame->ebp;
    }
}

void x86_start_kernel(x86_startup_info *info)
{
    console_register(&com1_console.console);
    declare_panic_hook(x86_do_backtrace, "Backtrace");
    install_panic_hook(&x86_do_backtrace_holder);

    mos_debug(x86_startup, "initrd %zu bytes, mbinfo at: " PTR_FMT ", magic " PTR_FMT, info->initrd_size, (ptr_t) info->mb_info, (ptr_t) info->mb_magic);

    const multiboot_info_t *mb_info = info->mb_info;
    initrd_size = info->initrd_size;

    if (initrd_size)
        initrd_paddr = ((x86_pgtable_entry *) (((x86_pgdir_entry *) x86_get_cr3())[MOS_X86_INITRD_VADDR >> 22].page_table_paddr << 12))->phys_addr << 12;

    x86_init_current_cpu_gdt();
    x86_idt_init();
    x86_init_current_cpu_tss();
    x86_irq_handler_init();

    if (mb_info->flags & MULTIBOOT_INFO_CMDLINE)
        strncpy(mos_cmdline, mb_info->cmdline, sizeof(mos_cmdline));

    const u32 memregion_count = mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    x86_pmm_region_setup(mb_info->mmap_addr, memregion_count);

    x86_mm_paging_init();

    mos_debug(x86_startup, "mapping kernel space...");
    x86_platform.k_code.npages = (ALIGN_UP_TO_PAGE((ptr_t) &__MOS_KERNEL_CODE_END) - ALIGN_DOWN_TO_PAGE((ptr_t) &__MOS_KERNEL_CODE_START)) / MOS_PAGE_SIZE;
    x86_platform.k_code = mm_map_pages(                                                                     //
        x86_platform.kernel_pgd,                                                                            //
        x86_platform.k_code.vaddr,                                                                          //
        pmm_reserve_frames(x86_platform.k_code.vaddr - MOS_KERNEL_START_VADDR, x86_platform.k_code.npages), //
        x86_platform.k_code.npages,                                                                         //
        x86_platform.k_code.flags                                                                           //
    );

    x86_platform.k_rodata.npages = (ALIGN_UP_TO_PAGE((ptr_t) &__MOS_KERNEL_RODATA_END) - ALIGN_DOWN_TO_PAGE((ptr_t) &__MOS_KERNEL_RODATA_START)) / MOS_PAGE_SIZE;
    x86_platform.k_rodata = mm_map_pages(                                                                       //
        x86_platform.kernel_pgd,                                                                                //
        x86_platform.k_rodata.vaddr,                                                                            //
        pmm_reserve_frames(x86_platform.k_rodata.vaddr - MOS_KERNEL_START_VADDR, x86_platform.k_rodata.npages), //
        x86_platform.k_rodata.npages,                                                                           //
        x86_platform.k_rodata.flags                                                                             //
    );

    x86_platform.k_rwdata.npages = (ALIGN_UP_TO_PAGE((ptr_t) &__MOS_KERNEL_RW_END) - ALIGN_DOWN_TO_PAGE((ptr_t) &__MOS_KERNEL_RW_START)) / MOS_PAGE_SIZE;
    x86_platform.k_rwdata = mm_map_pages(                                                                       //
        x86_platform.kernel_pgd,                                                                                //
        x86_platform.k_rwdata.vaddr,                                                                            //
        pmm_reserve_frames(x86_platform.k_rwdata.vaddr - MOS_KERNEL_START_VADDR, x86_platform.k_rwdata.npages), //
        x86_platform.k_rwdata.npages,                                                                           //
        x86_platform.k_rwdata.flags                                                                             //
    );

    mos_debug(x86_startup, "mapping bios memory area...");
    pmm_reserve_frames(X86_BIOS_MEMREGION_PADDR, x86_bios_block.npages);
    pmm_reserve_frames(X86_EBDA_MEMREGION_PADDR, x86_ebda_block.npages);
    x86_bios_block = mm_map_pages(x86_platform.kernel_pgd, x86_bios_block.vaddr, X86_BIOS_MEMREGION_PADDR, x86_bios_block.npages, x86_bios_block.flags);
    x86_ebda_block = mm_map_pages(x86_platform.kernel_pgd, x86_ebda_block.vaddr, X86_EBDA_MEMREGION_PADDR, x86_ebda_block.npages, x86_ebda_block.flags);

#if MOS_CONFIG(MOS_SMP)
    mos_debug(x86_startup, "copying memory for SMP boot...");
    x86_smp_copy_trampoline();
#endif

    x86_enable_paging_impl(((ptr_t) x86_kpg_infra->pgdir) - MOS_KERNEL_START_VADDR);
    mos_debug(x86_startup, "paging enabled");

    mos_kernel_mm_init(); // since then, we can use the kernel heap (kmalloc)

    if (initrd_size)
    {
        const size_t initrd_pgcount = ALIGN_UP_TO_PAGE(initrd_size) / MOS_PAGE_SIZE;
        pmm_reserve_frames(initrd_paddr, initrd_pgcount);
        const vmblock_t initrd_block = mm_map_pages(x86_platform.kernel_pgd, MOS_X86_INITRD_VADDR, initrd_paddr, initrd_pgcount, VM_READ | VM_GLOBAL);

        initrd_blockdev_t *initrd_blockdev = kzalloc(sizeof(initrd_blockdev_t));
        initrd_blockdev->blockdev = (blockdev_t){ .name = "initrd", .read = initrd_read };
        initrd_blockdev->vmblock = initrd_block;
        blockdev_register(&initrd_blockdev->blockdev);
    }

    mos_debug(x86_startup, "Parsing ACPI tables...");
    acpi_rsdp_t *rsdp = acpi_find_rsdp(BIOS_VADDR(X86_EBDA_MEMREGION_PADDR), EBDA_MEMREGION_SIZE);
    if (!rsdp)
    {
        rsdp = acpi_find_rsdp(BIOS_VADDR(X86_BIOS_MEMREGION_PADDR), BIOS_MEMREGION_SIZE);
        if (!rsdp)
            mos_panic("RSDP not found");
    }

    if (rsdp->xsdt_addr)
        mos_panic("XSDT not supported");

    const pmrange_t acpi_region = pmm_reserve_block(rsdp->v1.rsdt_addr);
    mm_map_pages(x86_platform.kernel_pgd, BIOS_VADDR(acpi_region.paddr), acpi_region.paddr, acpi_region.npages, VM_READ | VM_GLOBAL);

    acpi_parse_rsdt(rsdp);

    mos_debug(x86_startup, "Initializing APICs...");
    madt_parse_table();
    lapic_memory_setup();
    lapic_enable();
    pic_remap_irq();
    ioapic_init();

    x86_install_interrupt_handler(IRQ_TIMER, x86_timer_handler);
    x86_install_interrupt_handler(IRQ_KEYBOARD, x86_keyboard_handler);
    x86_install_interrupt_handler(IRQ_COM1, serial_irq_handler);

    ioapic_enable_interrupt(IRQ_TIMER, 0);
    ioapic_enable_interrupt(IRQ_KEYBOARD, 0);
    ioapic_enable_interrupt(IRQ_COM1, 0);

    current_cpu->id = x86_platform.boot_cpu_id = lapic_get_id();

#if MOS_CONFIG(MOS_SMP)
    x86_smp_start_all();
#endif

    mos_start_kernel(mos_cmdline);
}
