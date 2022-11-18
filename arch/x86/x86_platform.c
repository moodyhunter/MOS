// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/devices/initrd_blockdev.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/devices/serial_console.h"
#include "mos/x86/devices/text_mode_console.h"
#include "mos/x86/interrupt/pic.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/mm/pmem_freelist.h"
#include "mos/x86/tasks/context.h"
#include "mos/x86/x86_interrupt.h"

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

const uintptr_t mos_kernel_end = (uintptr_t) &__MOS_KERNEL_END;

void x86_keyboard_handler(u32 irq)
{
    MOS_ASSERT(irq == IRQ_KEYBOARD);
    int scancode = port_inb(0x60);
    char buf[9] = { 0 };
    for (int i = 7; i >= 0; i--)
    {
        buf[i] = '0' + (scancode & 1);
        scancode >>= 1;
    }

    pr_info("scancode: 0b%s", buf);
}

void do_backtrace(u32 max)
{
    static bool is_tracing = false;
    if (is_tracing)
        return;
    pr_info("Stack trace:");
    struct frame_t
    {
        struct frame_t *ebp;
        uintptr_t eip;
    } *frame = NULL;

    __asm__("movl %%ebp,%1" : "=r"(frame) : "r"(frame));
    for (u32 i = 0; frame && i < max; i++)
    {
        pr_warn("  " PTR_FMT, frame->eip);
        frame = frame->ebp;
    }
}

void x86_kpanic_hook()
{
    pmem_freelist_dump();
    const char *cpu_pagetable_source = current_cpu->pagetable.ptr == (uintptr_t) x86_kpg_infra ? "Kernel" : NULL;

    if (current_thread)
    {
        pr_emph("Current task: %s (tid: %d, pid: %d)", current_process->name, current_thread->tid, current_process->pid);
        pr_emph("Task Page Table:");
        x86_mm_dump_page_table(x86_get_pg_infra(current_process->pagetable));
        if (current_cpu->pagetable.ptr == current_process->pagetable.ptr)
            cpu_pagetable_source = "Current Process";
    }
    else
    {
        pr_emph("Kernel Page Table:");
        x86_mm_dump_page_table(x86_kpg_infra);
    }

    if (cpu_pagetable_source)
    {
        pr_emph("CPU Page Table: %s", cpu_pagetable_source);
    }
    else
    {
        pr_emph("CPU Page Table:");
        x86_mm_dump_page_table(x86_get_pg_infra(current_cpu->pagetable));
    }
    // do_backtrace(20);
}

void x86_start_kernel(x86_startup_info *info)
{
    multiboot_info_t *mb_info = info->mb_info;
    uintptr_t initrd_size = info->initrd_size;
    x86_disable_interrupts();
    console_register(&com1_console.console);

    x86_gdt_init();
    x86_idt_init();
    x86_tss_init();
    x86_irq_handler_init();

    pic_unmask_irq(IRQ_TIMER);
    pic_unmask_irq(IRQ_KEYBOARD);
    pic_unmask_irq(IRQ_COM1);
    pic_unmask_irq(IRQ_COM2);

    if (mb_info->flags & MULTIBOOT_INFO_CMDLINE)
        strncpy(mos_cmdline, mb_info->cmdline, sizeof(mos_cmdline));

    u32 count = mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    x86_mem_init(mb_info->mmap_addr, count);

    x86_mm_prepare_paging();
    current_cpu->pagetable.ptr = (uintptr_t) x86_kpg_infra;

    if (initrd_size)
    {
        reg_t cr3 = x86_get_cr3();
        const uintptr_t initrd_vaddr = MOS_X86_INITRD_VADDR;
        x86_pgtable_entry *table = (x86_pgtable_entry *) (((x86_pgdir_entry *) cr3)[initrd_vaddr >> 22].page_table_paddr << 12);
        const uintptr_t initrd_paddr = table->phys_addr << 12;
        const size_t initrd_n_pages = ALIGN_UP_TO_PAGE(initrd_size) / MOS_PAGE_SIZE;
        pg_map_pages(x86_kpg_infra, initrd_vaddr, initrd_paddr, initrd_n_pages, VM_GLOBAL);
    }

    // x86_acpi_init();
    // x86_smp_init();

    // ! map the bios memory area, should it be done like this?
    pr_info("mapping bios memory area...");
    for (u32 i = 0; i < x86_mem_regions_count; i++)
        if (x86_mem_regions[i].paddr == info->bios_region_start)
            x86_bios_region = &x86_mem_regions[i];

    memblock_t *bios_block = x86_bios_region;
    pg_do_map_pages(x86_kpg_infra, bios_block->paddr, bios_block->paddr, bios_block->size_bytes / MOS_PAGE_SIZE, VM_GLOBAL);

    x86_mm_enable_paging();

    mos_kernel_mm_init(); // since then, we can use the kernel heap (kmalloc)

    mos_install_kpanic_hook(x86_kpanic_hook);
    console_register(&vga_text_mode_console);
    x86_install_interrupt_handler(IRQ_COM1, serial_irq_handler);
    x86_install_interrupt_handler(IRQ_TIMER, x86_timer_handler);
    x86_install_interrupt_handler(IRQ_KEYBOARD, x86_keyboard_handler);

    if (initrd_size)
    {
        initrd_blockdev_t *initrd_blockdev = kmalloc(sizeof(initrd_blockdev_t));
        initrd_blockdev->memblock = (memblock_t){ .available = true, .vaddr = MOS_X86_INITRD_VADDR, .size_bytes = initrd_size };

        initrd_blockdev_preinstall(initrd_blockdev);
        blockdev_register(&initrd_blockdev->blockdev);
    }

    mos_start_kernel(mos_cmdline);
}

mos_platform_info_t x86_platform = {
    .regions = {
        .code_start = (uintptr_t) &__MOS_KERNEL_CODE_START,
        .code_end = (uintptr_t) &__MOS_KERNEL_CODE_END,
        .rodata_start = (uintptr_t) &__MOS_KERNEL_RODATA_START,
        .rodata_end = (uintptr_t) &__MOS_KERNEL_RODATA_END,
        .rw_start = (uintptr_t) &__MOS_KERNEL_RW_START,
        .rw_end = (uintptr_t) &__MOS_KERNEL_RW_END,
    },
};

mos_platform_info_t *const platform_info = &x86_platform;
