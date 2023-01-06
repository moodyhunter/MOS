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

static const vmblock_t x86_bios_block = {
    .npages = BIOS_MEMREGION_SIZE / MOS_PAGE_SIZE,
    .vaddr = BIOS_VADDR(X86_BIOS_MEMREGION_PADDR),
    .flags = VM_READ | VM_GLOBAL | VM_CACHE_DISABLED,
    .paddr = X86_BIOS_MEMREGION_PADDR,
};

static const vmblock_t x86_ebda_block = {
    .npages = EBDA_MEMREGION_SIZE / MOS_PAGE_SIZE,
    .vaddr = BIOS_VADDR(X86_EBDA_MEMREGION_PADDR),
    .flags = VM_READ | VM_GLOBAL | VM_CACHE_DISABLED,
    .paddr = X86_EBDA_MEMREGION_PADDR,
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

    console_register(&com1_console.console);
    pr_info("mos_startup_info: initrd %zu bytes, mbinfo at: " PTR_FMT ", magic " PTR_FMT, info->initrd_size, (uintptr_t) info->mb_info, (uintptr_t) info->mb_magic);

    const multiboot_info_t *mb_info = info->mb_info;
    const uintptr_t initrd_size = info->initrd_size;
    const uintptr_t initrd_paddr = ((x86_pgtable_entry *) (((x86_pgdir_entry *) x86_get_cr3())[MOS_X86_INITRD_VADDR >> 22].page_table_paddr << 12))->phys_addr << 12;

    x86_gdt_init();
    x86_idt_init();
    x86_tss_init();
    x86_irq_handler_init();

    if (mb_info->flags & MULTIBOOT_INFO_CMDLINE)
        strncpy(mos_cmdline, mb_info->cmdline, sizeof(mos_cmdline));

    current_cpu->pagetable.pgd = x86_platform.kernel_pgd.pgd = (uintptr_t) x86_kpg_infra;
    current_cpu->pagetable.um_page_map = x86_platform.kernel_pgd.um_page_map = NULL; // a kernel page table does not have a user-mode page map

    const u32 memregion_count = mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    x86_mem_init(mb_info->mmap_addr, memregion_count);
    MOS_ASSERT_X(x86_platform.mem_regions.count != memregion_count, "x86_mem_init() failed to initialize all memory regions");
    mos_pmm_setup();

    // initialize the page directory
    memzero(x86_kpg_infra, sizeof(x86_pg_infra_t));

    pr_info("mapping kernel space...");
    mm_map_pages(x86_platform.kernel_pgd, x86_platform.k_code);
    mm_map_pages(x86_platform.kernel_pgd, x86_platform.k_rodata);
    mm_map_pages(x86_platform.kernel_pgd, x86_platform.k_rwdata);

    pr_info("mapping bios memory area...");
    mm_map_allocated_pages(x86_platform.kernel_pgd, x86_bios_block);
    mm_map_allocated_pages(x86_platform.kernel_pgd, x86_ebda_block);

    pr_info("reserving memory for AP boot...");
    x86_smp_copy_trampoline();

    x86_mm_enable_paging();
    mos_kernel_mm_init(); // since then, we can use the kernel heap (kmalloc)

    if (initrd_size)
    {
        vmblock_t initrd_block = (vmblock_t){
            .vaddr = MOS_X86_INITRD_VADDR,
            .flags = VM_READ | VM_GLOBAL,
            .paddr = initrd_paddr,
            .npages = ALIGN_UP_TO_PAGE(initrd_size) / MOS_PAGE_SIZE,
        };
        mm_map_pages(x86_platform.kernel_pgd, initrd_block);

        initrd_blockdev_t *initrd_blockdev = kzalloc(sizeof(initrd_blockdev_t));
        initrd_blockdev->blockdev = (blockdev_t){ .name = "initrd", .read = initrd_read };
        initrd_blockdev->memblock = (memregion_t){ .available = true, .address = MOS_X86_INITRD_VADDR, .size_bytes = initrd_size };
        blockdev_register(&initrd_blockdev->blockdev);
    }

    pr_info("Parsing ACPI tables...");
    acpi_rsdp_t *rsdp = acpi_find_rsdp(BIOS_VADDR(X86_EBDA_MEMREGION_PADDR), EBDA_MEMREGION_SIZE);
    if (!rsdp)
    {
        rsdp = acpi_find_rsdp(BIOS_VADDR(X86_BIOS_MEMREGION_PADDR), BIOS_MEMREGION_SIZE);
        if (!rsdp)
            mos_panic("RSDP not found");
    }

    // find the RSDT and map the whole region it's in
    for (u32 i = 0; i < x86_platform.mem_regions.count; i++)
    {
        memregion_t *this_region = &x86_platform.mem_regions.regions[i];
        if (rsdp->xsdt_addr)
            mos_panic("XSDT not supported");
        if (rsdp->v1.rsdt_addr >= this_region->address && rsdp->v1.rsdt_addr < this_region->address + this_region->size_bytes)
        {
            vmblock_t bios_vmblock = (vmblock_t){
                .npages = this_region->size_bytes / MOS_PAGE_SIZE,
                .vaddr = BIOS_VADDR(this_region->address),
                .flags = VM_READ | VM_GLOBAL,
                .paddr = this_region->address,
            };
            mm_map_allocated_pages(x86_platform.kernel_pgd, bios_vmblock);
            this_region->address = BIOS_VADDR(this_region->address);
            break;
        }
    }

    acpi_parse_rsdt(rsdp);

    pr_info("Initializing APICs...");
    madt_parse_table();
    lapic_memory_setup();
    lapic_enable();
    pic_remap_irq();
    ioapic_init();

    pr_info("Starting APs...");
    current_cpu->id = x86_platform.boot_cpu_id = lapic_get_id();
    x86_smp_start_all();

    mos_install_kpanic_hook(x86_kpanic_hook);

    x86_install_interrupt_handler(IRQ_TIMER, x86_timer_handler);
    x86_install_interrupt_handler(IRQ_KEYBOARD, x86_keyboard_handler);
    x86_install_interrupt_handler(IRQ_COM1, serial_irq_handler);

    ioapic_enable_interrupt(IRQ_TIMER, 0);
    ioapic_enable_interrupt(IRQ_KEYBOARD, 0);
    ioapic_enable_interrupt(IRQ_COM1, 0);

    // TODO: move this to a userspace program
    pr_info("paging: mapping video memory...");
    vmblock_t video_block = (vmblock_t){
        .npages = 1,
        .vaddr = BIOS_VADDR(X86_VIDEO_DEVICE_PADDR),
        .flags = VM_RW | VM_GLOBAL,
        .paddr = X86_VIDEO_DEVICE_PADDR,
    };
    mm_map_allocated_pages(current_cpu->pagetable, video_block);
    console_register(&vga_text_mode_console);

    mos_start_kernel(mos_cmdline);
}

mos_platform_info_t x86_platform;
mos_platform_info_t *const platform_info = &x86_platform;
