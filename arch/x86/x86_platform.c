// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/drivers/port.h"
#include "mos/x86/drivers/serial.h"
#include "mos/x86/drivers/serial_console.h"
#include "mos/x86/drivers/text_mode_console.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging.h"

// defined in the linker script 'multiboot.ld'
extern const char __MOS_SECTION_KERNEL_START;
extern const char __MOS_SECTION_KERNEL_END;

static serial_console_t com1_console = {
    .device = { .port = COM1, .baud_rate = 115200, .char_length = CHAR_LENGTH_8, .stop_bits = STOP_BITS_1, .parity = PARITY_EVEN },
    .console = { .name = "Serial Console for COM1", .caps = CONSOLE_CAP_SETUP, .setup = serial_console_setup },
};

// !! TODO: Use kmalloc to allocate the handler_descriptor
static irq_handler_descriptor_t com1_handler = {
    .list_node = MOS_LIST_NODE_INIT(com1_handler),
    .handler = serial_irq_handler,
};

void x86_start_kernel(u32 magic, multiboot_info_t *mb_info)
{
    mos_register_console(&vga_text_mode_console);
    mos_register_console(&com1_console.console);

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        mos_panic("invalid magic number: %x", magic);

    if (!(mb_info->flags & MULTIBOOT_INFO_MEM_MAP))
        mos_panic("no memory map");

    x86_disable_interrupts();

    x86_gdt_init();
    x86_idt_init();
    x86_irq_handler_init();

    // I don't like the timer interrupt, so disable it.
    x86_irq_mask(IRQ_TIMER);
    x86_irq_unmask(IRQ_KEYBOARD);
    x86_irq_unmask(IRQ_COM1);
    x86_irq_unmask(IRQ_COM2);
    x86_irq_unmask(IRQ_PS2_MOUSE);
    x86_install_interrupt_handler(IRQ_COM1, &com1_handler);

    u32 count = mb_info->mmap_length / sizeof(multiboot_mmap_entry_t);
    x86_setup_mem(mb_info->mmap_addr, count);

    mos_init_info_t init;
    init.cmdline = mb_info->cmdline;
    mos_start_kernel(&init);
}

void x86_disable_interrupts()
{
    __asm__ volatile("cli");
}

void x86_enable_interrupts()
{
    __asm__ volatile("sti");
}

bool x86_install_interrupt_handler(u32 irq, irq_handler_descriptor_t *handler)
{
    list_node_append(&irq_handlers[irq], list_node(handler));
    return true;
}

void __noreturn x86_shutdown_vm()
{
    x86_disable_interrupts();
    port_outw(0x604, 0x2000);
    while (1)
        ;
}

mos_platform_t mos_platform = {
    .kernel_start = (void *) &__MOS_SECTION_KERNEL_START,
    .kernel_end = (void *) &__MOS_SECTION_KERNEL_END,

    .shutdown = x86_shutdown_vm,
    .interrupt_disable = x86_disable_interrupts,
    .interrupt_enable = x86_enable_interrupts,
    .irq_install_handler = x86_install_interrupt_handler,

    // memory management
    .mm_page_size = X86_PAGE_SIZE,
    .mm_setup_paging = x86_enable_paging,
    .mm_alloc_page = x86_alloc_page,
    .mm_free_page = x86_free_page,
};
