// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/panic.h"
#include "mos/printk.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/cpu/smp.h"
#include "mos/x86/drivers/port.h"
#include "mos/x86/drivers/serial_console.h"
#include "mos/x86/drivers/text_mode_console.h"
#include "mos/x86/interrupt/pic.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging.h"

const uintptr_t x86_kernel_start = (uintptr_t) &__MOS_SECTION_KERNEL_START;
const uintptr_t x86_kernel_end = (uintptr_t) &__MOS_SECTION_KERNEL_END;

static serial_console_t com1_console = {
    .device = { .port = COM1, .baud_rate = BAUD_RATE_115200, .char_length = CHAR_LENGTH_8, .stop_bits = STOP_BITS_1, .parity = PARITY_EVEN },
    .console = { .name = "Serial Console on COM1", .caps = CONSOLE_CAP_SETUP | CONSOLE_CAP_COLOR, .setup = serial_console_setup },
};

mos_platform_cpu_info_t x86_cpu_info = { 0 };

static char mos_cmdline[512];

memblock_t *x86_mem_find_bios_block()
{
    memblock_t *bios_memblock = NULL;
    for (u32 i = 0; i < x86_mem_regions_count; i++)
    {
        const uintptr_t region_start_addr = x86_mem_regions[i].paddr;
        if (region_start_addr < (uintptr_t) x86_acpi_rsdt && region_start_addr + x86_mem_regions[i].size_bytes > (uintptr_t) x86_acpi_rsdt)
        {
            bios_memblock = &x86_mem_regions[i];
            break;
        }
    }

    if (!bios_memblock)
        mos_panic("could not find bios memory area");
    return bios_memblock;
}

void x86_start_kernel(u32 magic, multiboot_info_t *mb_info)
{
    x86_disable_interrupts();
    mos_register_console(&com1_console.console);

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        mos_panic("invalid magic number: %x", magic);

    if (!(mb_info->flags & MULTIBOOT_INFO_MEM_MAP))
        mos_panic("no memory map");

    x86_gdt_init();
    x86_idt_init();
    x86_irq_handler_init();

    // I don't like the timer interrupt, so disable it.
    pic_mask_irq(IRQ_TIMER);
    pic_unmask_irq(IRQ_KEYBOARD);
    pic_unmask_irq(IRQ_COM1);
    pic_unmask_irq(IRQ_COM2);
    pic_unmask_irq(IRQ_PS2_MOUSE);

    strncpy(mos_cmdline, mb_info->cmdline, sizeof(mos_cmdline));

    u32 count = mb_info->mmap_length / sizeof(multiboot_mmap_entry_t);
    x86_mem_init(mb_info->mmap_addr, count);
    x86_mm_prepare_paging();

    x86_acpi_init();
    x86_smp_init();

    // ! map the bios memory area, should it be done like this?
    pr_info("mapping bios memory area...");
    memblock_t *bios_memblock = x86_mem_find_bios_block();
    vm_map_page_range_no_freelist(bios_memblock->paddr, bios_memblock->paddr, bios_memblock->size_bytes / X86_PAGE_SIZE, VM_PRESENT);

    mos_init_info_t init;
    init.cmdline = mos_cmdline;
    x86_mm_enable_paging();
    mos_start_kernel(&init);
}

void x86_kpanic_hook()
{
    pmem_freelist_dump();
}

void x86_post_kernel_init(mos_init_info_t *info)
{
    MOS_UNUSED(info);
    mos_install_kpanic_hook(x86_kpanic_hook);
}

void x86_setup_devices(mos_init_info_t *init_info)
{
    MOS_UNUSED(init_info);
    mos_register_console(&vga_text_mode_console);
    x86_install_interrupt_handler(IRQ_COM1, &serial_irq_handler);
}

void x86_disable_interrupts()
{
    __asm__ volatile("cli");
}

void x86_enable_interrupts()
{
    __asm__ volatile("sti");
}

bool x86_install_interrupt_handler(u32 irq, void (*handler)(u32 irq))
{
    irq_handler_descriptor_t *desc = kmalloc(sizeof(irq_handler_descriptor_t));
    desc->handler = handler;
    list_node_append(&irq_handlers[irq], list_node(desc));
    return true;
}

void __noreturn x86_shutdown_vm()
{
    x86_disable_interrupts();
    port_outw(0x604, 0x2000);
    while (1)
        ;
}

const mos_platform_t mos_platform = {
    .kernel_start = &__MOS_SECTION_KERNEL_START,
    .kernel_end = &__MOS_SECTION_KERNEL_END,

    .cpu_info = &x86_cpu_info,

    .post_init = x86_post_kernel_init,
    .devices_setup = x86_setup_devices,

    .shutdown = x86_shutdown_vm,
    .interrupt_disable = x86_disable_interrupts,
    .interrupt_enable = x86_enable_interrupts,
    .halt_cpu = x86_cpu_halt,
    .irq_handler_install = x86_install_interrupt_handler,
    .irq_handler_remove = NULL,

    // memory management
    .mm_page_size = X86_PAGE_SIZE,
    .mm_page_allocate = x86_mm_alloc_page,
    .mm_page_free = x86_mm_free_page,
};
