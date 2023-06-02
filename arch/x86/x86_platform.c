// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <stdlib.h>
#include <string.h>

static u8 com1_buf[MOS_PAGE_SIZE] __aligned(MOS_PAGE_SIZE) = { 0 };

static serial_console_t com1_console = {
    .device =
        &(serial_device_t){
            .port = COM1,
            .baud_rate = BAUD_RATE_115200,
            .char_length = CHAR_LENGTH_8,
            .stop_bits = STOP_BITS_1,
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
    },
    .fg = LightBlue,
    .bg = Black,
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

static initrd_blockdev_t initrd_blockdev;

mos_platform_info_t *const platform_info = &x86_platform;
mos_platform_info_t x86_platform = { 0 };

static void x86_keyboard_handler(u32 irq)
{
    MOS_ASSERT(irq == IRQ_KEYBOARD);
    int scancode = port_inb(0x60);

    pr_info("Keyboard scancode: %x", scancode);
}

static void x86_com1_handler(u32 irq)
{
    MOS_ASSERT(irq == IRQ_COM1);
    char c = '\0';
    serial_device_read(com1_console.device, &c, 1);
    console_putc(&com1_console.con, c);
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
        bool mapped = mm_get_is_mapped(current_cpu->pagetable, (ptr_t) frame);
        if (!mapped)
        {
            pr_warn("  %-2d" PTR_FMT ": <corrupted>, aborting backtrace", i, (ptr_t) frame);
            break;
        }
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
    console_register(&com1_console.con, MOS_PAGE_SIZE);
    const multiboot_info_t *mb_info = info->mb_info;
    size_t initrd_npages = 0;
    pfn_t initrd_pfn = 0;

    if (mb_info->flags & MULTIBOOT_INFO_MODS && mb_info->mods_count != 0)
    {
        const multiboot_module_t *mod = (const multiboot_module_t *) mb_info->mods_addr;
        initrd_npages = ALIGN_UP_TO_PAGE(mod->mod_end - mod->mod_start) / MOS_PAGE_SIZE;
        initrd_pfn = ALIGN_DOWN_TO_PAGE(mod->mod_start) / MOS_PAGE_SIZE;
        mos_debug(x86_startup, "initrd at " PFN_FMT ", size %zu pages", initrd_pfn, initrd_npages);
    }

    mos_cmdline_parse(mb_info->cmdline);
    setup_invoke_earlysetup();

    declare_panic_hook(x86_do_backtrace, "Backtrace");
    install_panic_hook(&x86_do_backtrace_holder);

    x86_init_current_cpu_gdt();
    x86_idt_init();
    x86_init_current_cpu_tss();
    x86_irq_handler_init();

#if MOS_CONFIG(MOS_SMP)
    mos_debug(x86_startup, "copying memory for SMP boot...");
    x86_smp_copy_trampoline();
#endif

    x86_mm_paging_init();

    mos_debug(x86_startup, "mapping kernel space...");

    x86_platform.k_code = mm_early_map_kernel_pages(                                                         //
        (ptr_t) __MOS_KERNEL_CODE_START,                                                                     //
        ((ptr_t) __MOS_KERNEL_CODE_START - MOS_KERNEL_START_VADDR) / MOS_PAGE_SIZE,                          //
        (ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_CODE_END) - (ptr_t) __MOS_KERNEL_CODE_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_GLOBAL                                                                                  //
    );

    x86_platform.k_rodata = mm_early_map_kernel_pages(                                                           //
        (ptr_t) __MOS_KERNEL_RODATA_START,                                                                       //
        ((ptr_t) __MOS_KERNEL_RODATA_START - MOS_KERNEL_START_VADDR) / MOS_PAGE_SIZE,                            //
        (ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_RODATA_END) - (ptr_t) __MOS_KERNEL_RODATA_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_GLOBAL                                                                                      //
    );

    x86_platform.k_rwdata = mm_early_map_kernel_pages(                                                   //
        (ptr_t) __MOS_KERNEL_RW_START,                                                                   //
        ((ptr_t) __MOS_KERNEL_RW_START - MOS_KERNEL_START_VADDR) / MOS_PAGE_SIZE,                        //
        (ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_RW_END) - (ptr_t) __MOS_KERNEL_RW_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_WRITE | VM_GLOBAL                                                                   //
    );

    // make our own copy of the multiboot info
    // switching to our new page table will ruin the old one
    const size_t mmap_count = mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    multiboot_memory_map_t mmaps[mmap_count];
    memcpy(mmaps, (void *) mb_info->mmap_addr, mb_info->mmap_length);

    x86_enable_paging_impl(((ptr_t) x86_kpg_infra->pgdir) - MOS_KERNEL_START_VADDR);

    mos_debug(x86_startup, "setting up physical memory manager...");
    x86_pmm_region_setup(mmaps, mmap_count, initrd_pfn, initrd_npages);

    pmm_reserve_addresses(x86_platform.k_code.vaddr - MOS_KERNEL_START_VADDR, x86_platform.k_code.npages);
    pmm_reserve_addresses(x86_platform.k_rodata.vaddr - MOS_KERNEL_START_VADDR, x86_platform.k_rodata.npages);
    pmm_reserve_addresses(x86_platform.k_rwdata.vaddr - MOS_KERNEL_START_VADDR, x86_platform.k_rwdata.npages);

    mos_debug(x86_startup, "mapping bios memory area...");
    // pmm_reserve_frames(X86_BIOS_MEMREGION_PADDR / MOS_PAGE_SIZE, x86_bios_block.npages);
    // pmm_reserve_frames(X86_EBDA_MEMREGION_PADDR / MOS_PAGE_SIZE, x86_ebda_block.npages);
    x86_bios_block = mm_map_pages(x86_platform.kernel_pgd, x86_bios_block.vaddr, X86_BIOS_MEMREGION_PADDR / MOS_PAGE_SIZE, x86_bios_block.npages, x86_bios_block.flags);
    x86_ebda_block = mm_map_pages(x86_platform.kernel_pgd, x86_ebda_block.vaddr, X86_EBDA_MEMREGION_PADDR / MOS_PAGE_SIZE, x86_ebda_block.npages, x86_ebda_block.flags);

    if (initrd_npages)
    {
        pmm_reserve_frames(initrd_pfn, initrd_npages);
        initrd_blockdev.blockdev = (blockdev_t){ .name = "initrd", .read = initrd_read };
        initrd_blockdev.vmblock = mm_map_pages(x86_platform.kernel_pgd, MOS_X86_INITRD_VADDR, initrd_pfn, initrd_npages, VM_READ | VM_GLOBAL);
    }

    mos_kernel_mm_init(); // we can now use the kernel heap (kmalloc)

    if (initrd_npages)
        blockdev_register(&initrd_blockdev.blockdev); // must be done after kmalloc is initialized

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

    const pmm_region_t *acpi_region = pmm_find_reserved_region(rsdp->v1.rsdt_addr);
    MOS_ASSERT_X(acpi_region && acpi_region->reserved, "ACPI region not found or not reserved");
    const ptr_t phyaddr = acpi_region->pfn_start * MOS_PAGE_SIZE;
    mm_map_pages(x86_platform.kernel_pgd, BIOS_VADDR(phyaddr), acpi_region->pfn_start, acpi_region->nframes, VM_READ | VM_GLOBAL);

    acpi_parse_rsdt(rsdp);

    mos_debug(x86_startup, "Initializing APICs...");
    madt_parse_table();
    lapic_memory_setup();
    lapic_enable();
    pic_remap_irq();
    ioapic_init();

    x86_install_interrupt_handler(IRQ_TIMER, x86_timer_handler);
    x86_install_interrupt_handler(IRQ_KEYBOARD, x86_keyboard_handler);
    x86_install_interrupt_handler(IRQ_COM1, x86_com1_handler);

    ioapic_enable_interrupt(IRQ_TIMER, 0);
    ioapic_enable_interrupt(IRQ_KEYBOARD, 0);
    ioapic_enable_interrupt(IRQ_COM1, 0);

    current_cpu->id = x86_platform.boot_cpu_id = lapic_get_id();

#if MOS_CONFIG(MOS_SMP)
    x86_smp_start_all();
#endif

    mos_start_kernel();
}
