// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "lib/string.h"
#include "mos/constants.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pmalloc.h"
#include "mos/mos_global.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/acpi/madt.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/cpu/smp.h"
#include "mos/x86/devices/initrd_blockdev.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/devices/serial_console.h"
#include "mos/x86/devices/text_mode_console.h"
#include "mos/x86/interrupt/apic.h"
#include "mos/x86/interrupt/pic.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
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
    const char *cpu_pagetable_source = current_cpu->pagetable.pgd == (uintptr_t) x86_kpg_infra ? "Kernel" : NULL;

    if (current_thread)
    {
        pr_emph("Current task: %s (tid: %d, pid: %d)", current_process->name, current_thread->tid, current_process->pid);
        pr_emph("Task Page Table:");
        x86_mm_dump_page_table(x86_get_pg_infra(current_process->pagetable));
        if (current_cpu->pagetable.pgd == current_process->pagetable.pgd)
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
    x86_platform.k_code.vaddr = (uintptr_t) &__MOS_KERNEL_CODE_START;
    x86_platform.k_code.npages = (ALIGN_UP_TO_PAGE((uintptr_t) &__MOS_KERNEL_CODE_END) - ALIGN_DOWN_TO_PAGE((uintptr_t) &__MOS_KERNEL_CODE_START)) / MOS_PAGE_SIZE;
    x86_platform.k_code.paddr = (uintptr_t) &__MOS_KERNEL_CODE_START - MOS_KERNEL_START_VADDR;
    x86_platform.k_code.flags = VM_GLOBAL | VM_READ | VM_EXEC;

    x86_platform.k_rwdata.vaddr = (uintptr_t) &__MOS_KERNEL_RW_START;
    x86_platform.k_rwdata.npages = (ALIGN_UP_TO_PAGE((uintptr_t) &__MOS_KERNEL_RW_END) - ALIGN_DOWN_TO_PAGE((uintptr_t) &__MOS_KERNEL_RW_START)) / MOS_PAGE_SIZE;
    x86_platform.k_rwdata.paddr = (uintptr_t) &__MOS_KERNEL_RW_START - MOS_KERNEL_START_VADDR;
    x86_platform.k_rwdata.flags = VM_GLOBAL | VM_READ | VM_WRITE;

    x86_platform.k_rodata.vaddr = (uintptr_t) &__MOS_KERNEL_RODATA_START;
    x86_platform.k_rodata.npages = (ALIGN_UP_TO_PAGE((uintptr_t) &__MOS_KERNEL_RODATA_END) - ALIGN_DOWN_TO_PAGE((uintptr_t) &__MOS_KERNEL_RODATA_START)) / MOS_PAGE_SIZE;
    x86_platform.k_rodata.paddr = (uintptr_t) &__MOS_KERNEL_RODATA_START - MOS_KERNEL_START_VADDR;
    x86_platform.k_rodata.flags = VM_GLOBAL | VM_READ;

    multiboot_info_t *mb_info = info->mb_info;
    uintptr_t initrd_size = info->initrd_size;
    x86_disable_interrupts();
    console_register(&com1_console.console);

    x86_gdt_init();
    x86_idt_init();
    x86_tss_init();
    x86_irq_handler_init();

    if (mb_info->flags & MULTIBOOT_INFO_CMDLINE)
        strncpy(mos_cmdline, mb_info->cmdline, sizeof(mos_cmdline));

    current_cpu->pagetable.pgd = (uintptr_t) x86_kpg_infra;
    current_cpu->pagetable.um_page_map = NULL; // a kernel page table does not have a user-mode page map
    x86_platform.kernel_pgd = current_cpu->pagetable;

    u32 count = mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    x86_mem_init(mb_info->mmap_addr, count);

    pr_info("paging: setting up physical memory freelist...");
    mos_pmalloc_setup();

    x86_mm_prepare_paging();

    if (initrd_size)
    {
        reg_t cr3 = x86_get_cr3();
        const volatile x86_pgtable_entry *table = (x86_pgtable_entry *) (((x86_pgdir_entry *) cr3)[MOS_X86_INITRD_VADDR >> 22].page_table_paddr << 12);

        vmblock_t initrd_block = (vmblock_t){
            .npages = ALIGN_UP_TO_PAGE(initrd_size) / MOS_PAGE_SIZE,
            .vaddr = MOS_X86_INITRD_VADDR,
            .flags = VM_READ | VM_GLOBAL,
            .paddr = table->phys_addr << 12,
        };

        mm_map_pages(current_cpu->pagetable, initrd_block);
    }

    // ! map the bios memory area, should it be done like this?
    pr_info("mapping bios memory area...");
    for (u32 i = 0; i < x86_platform.mem_regions.count; i++)
    {
        if (x86_platform.mem_regions.regions[i].address != info->bios_region_start)
            continue;

        memregion_t *bios_block = &x86_platform.mem_regions.regions[i];
        vmblock_t bios_vmblock = (vmblock_t){
            .npages = bios_block->size_bytes / MOS_PAGE_SIZE,
            .vaddr = BIOS_VADDR(bios_block->address),
            .flags = VM_READ | VM_GLOBAL,
            .paddr = bios_block->address,
        };
        mm_map_allocated_pages(current_cpu->pagetable, bios_vmblock);
        bios_block->address = BIOS_VADDR(bios_block->address);
    }

    pr_info("Parsing ACPI tables...");
    x86_acpi_init();

    pr_info("Initializing APICs...");
    acpi_parse_madt();
    lapic_memory_setup();
    lapic_enable();
    ioapic_init();

    pr_info("Starting APs...");
    x86_platform.boot_cpu_id = lapic_get_id();
    per_cpu(x86_platform.cpu)->id = lapic_get_id();
    x86_smp_init();

    x86_mm_enable_paging();

    mos_kernel_mm_init(); // since then, we can use the kernel heap (kmalloc)

    mos_install_kpanic_hook(x86_kpanic_hook);

    // map video memory
    pr_info("paging: mapping video memory...");
    vmblock_t video_block = (vmblock_t){
        .npages = 1,
        .vaddr = BIOS_VADDR(X86_VIDEO_DEVICE_PADDR),
        .flags = VM_RW | VM_GLOBAL,
        .paddr = X86_VIDEO_DEVICE_PADDR,
    };
    platform_mm_map_pages(current_cpu->pagetable, video_block); // the use platform_mm_map_pages is intentional (no pmem list, no pagemap)
    console_register(&vga_text_mode_console);

    x86_install_interrupt_handler(IRQ_TIMER, x86_timer_handler);
    x86_install_interrupt_handler(IRQ_KEYBOARD, x86_keyboard_handler);
    x86_install_interrupt_handler(IRQ_COM1, serial_irq_handler);

    ioapic_enable_interrupt(IRQ_TIMER, 0);
    ioapic_enable_interrupt(IRQ_KEYBOARD, 0);
    ioapic_enable_interrupt(IRQ_COM1, 0);

    if (initrd_size)
    {
        initrd_blockdev_t *initrd_blockdev = kmalloc(sizeof(initrd_blockdev_t));
        initrd_blockdev->memblock = (memregion_t){ .available = true, .address = MOS_X86_INITRD_VADDR, .size_bytes = initrd_size };

        initrd_blockdev_preinstall(initrd_blockdev);
        blockdev_register(&initrd_blockdev->blockdev);
    }

    mos_start_kernel(mos_cmdline);
}

mos_platform_info_t x86_platform;
mos_platform_info_t *const platform_info = &x86_platform;
