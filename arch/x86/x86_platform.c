// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/panic.h"
#include "mos/printk.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/drivers/port.h"
#include "mos/x86/drivers/serial_console.h"
#include "mos/x86/drivers/text_mode_console.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging.h"

const uintptr_t x86_kernel_start = (uintptr_t) &__MOS_SECTION_KERNEL_START;
const uintptr_t x86_kernel_end = (uintptr_t) &__MOS_SECTION_KERNEL_END;

static serial_console_t com1_console = {
    .device = { .port = COM1, .baud_rate = BAUD_RATE_115200, .char_length = CHAR_LENGTH_8, .stop_bits = STOP_BITS_1, .parity = PARITY_EVEN },
    .console = { .name = "Serial Console on COM1", .caps = CONSOLE_CAP_SETUP | CONSOLE_CAP_COLOR, .setup = serial_console_setup },
};

static char mos_cmdline[512];

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

    x86_cpuid_dump();

    strncpy(mos_cmdline, mb_info->cmdline, sizeof(mos_cmdline));

    u32 count = mb_info->mmap_length / sizeof(multiboot_mmap_entry_t);
    x86_mem_init(mb_info->mmap_addr, count);
    x86_mm_prepare_paging();
    x86_mm_enable_paging();

    mos_init_info_t init;
    init.cmdline = mos_cmdline;
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
    // I don't like the timer interrupt, so disable it.
    x86_irq_mask(IRQ_TIMER);
    x86_irq_unmask(IRQ_KEYBOARD);
    x86_irq_unmask(IRQ_COM1);
    x86_irq_unmask(IRQ_COM2);
    x86_irq_unmask(IRQ_PS2_MOUSE);
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

    .post_init = x86_post_kernel_init,
    .devices_setup = x86_setup_devices,

    .shutdown = x86_shutdown_vm,
    .interrupt_disable = x86_disable_interrupts,
    .interrupt_enable = x86_enable_interrupts,
    .irq_handler_install = x86_install_interrupt_handler,
    .irq_handler_remove = NULL,

    // memory management
    .mm_page_size = X86_PAGE_SIZE,
    .mm_page_allocate = x86_mm_alloc_page,
    .mm_page_free = x86_mm_free_page,
};
