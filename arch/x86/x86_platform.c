// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/types.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/smp.h"
#include "mos/x86/devices/initrd_blockdev.h"
#include "mos/x86/drivers/serial_console.h"
#include "mos/x86/drivers/text_mode_console.h"
#include "mos/x86/interrupt/pic.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/mm/pmem_freelist.h"
#include "mos/x86/tasks/context.h"

static char mos_cmdline[512];
static serial_console_t com1_console = {
    .device = { .port = COM1, .baud_rate = BAUD_RATE_115200, .char_length = CHAR_LENGTH_8, .stop_bits = STOP_BITS_1, .parity = PARITY_EVEN },
    .console = { .name = "Serial Console on COM1", .caps = CONSOLE_CAP_SETUP | CONSOLE_CAP_COLOR, .setup = serial_console_setup },
};

const uintptr_t mos_kernel_end = (uintptr_t) &__MOS_KERNEL_END;

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
    x86_mm_dump_page_table(x86_kpg_infra);
    do_backtrace(20);
}

memblock_t x86_settle_initrd(multiboot_info_t *mb_info)
{
    memblock_t initrd_region = { 0 };
    if (mb_info->flags & MULTIBOOT_INFO_MODS)
    {
        for (size_t i = 0; i < mb_info->mods_count; i++)
        {
            multiboot_module_t *m = ((multiboot_module_t *) mb_info->mods_addr) + i;
            if (m->mod_start == 0)
                continue;
            if (m->cmdline)
                pr_info("initrd: " PTR_FMT "-" PTR_FMT ": %s", (uintptr_t) m->mod_start, (uintptr_t) m->mod_end, (const char *) m->cmdline);

            initrd_region.paddr = m->mod_start;
            initrd_region.vaddr = m->mod_start;
            initrd_region.size_bytes = m->mod_end - m->mod_start;
            size_t pmem_freelist_size = pmem_freelist_getsize();
            // move initrd to the first available memory area
            uintptr_t new_initrd_addr = X86_ALIGN_UP_TO_PAGE(mos_kernel_end + pmem_freelist_size);
            pr_info2("moving initrd from " PTR_FMT " to " PTR_FMT, initrd_region.paddr, new_initrd_addr);
            memmove((void *) new_initrd_addr, (void *) initrd_region.paddr, initrd_region.size_bytes);
            initrd_region.paddr = new_initrd_addr;
            initrd_region.vaddr = new_initrd_addr;
            break;
        }
    }
    else
    {
        mos_warn("no initrd provided");
    }
    return initrd_region;
}

void x86_start_kernel(u32 magic, multiboot_info_t *mb_info)
{
    x86_disable_interrupts();
    mos_install_console(&com1_console.console);

    x86_gdt_init();
    x86_idt_init();
    x86_tss_init();
    x86_irq_handler_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        mos_panic("invalid magic number: %x", magic);

    if (!(mb_info->flags & MULTIBOOT_INFO_MEM_MAP))
        mos_panic("no memory map");

    // I don't like the timer interrupt, so disable it.
    pic_unmask_irq(IRQ_TIMER);
    pic_unmask_irq(IRQ_KEYBOARD);
    pic_unmask_irq(IRQ_COM1);
    pic_unmask_irq(IRQ_COM2);
    pic_unmask_irq(IRQ_PS2_MOUSE);

    if (mb_info->flags & MULTIBOOT_INFO_CMDLINE)
        strncpy(mos_cmdline, mb_info->cmdline, sizeof(mos_cmdline));

    u32 count = mb_info->mmap_length / sizeof(multiboot_mmap_entry_t);
    x86_mem_init(mb_info->mmap_addr, count);

    // ! The initrd was placed in .bss, firstly we have to move it somewhere else.
    memblock_t initrd_memblock = x86_settle_initrd(mb_info);

    x86_mm_prepare_paging();
    x86_platform.kernel_pg.ptr = (uintptr_t) x86_kpg_infra;

    if (initrd_memblock.size_bytes)
    {
        uintptr_t start_addr = X86_ALIGN_DOWN_TO_PAGE(initrd_memblock.paddr);
        uintptr_t end_addr = X86_ALIGN_UP_TO_PAGE(initrd_memblock.size_bytes + initrd_memblock.paddr);
        pg_map_pages(x86_kpg_infra, start_addr, start_addr, (end_addr - start_addr) / X86_PAGE_SIZE, VM_GLOBAL | VM_USERMODE);
    }

    x86_acpi_init();
    // map the lapic to the kernel page table
    pg_do_map_pages(x86_kpg_infra, x86_acpi_madt->lapic_addr, x86_acpi_madt->lapic_addr, 1, VM_GLOBAL);

    x86_smp_init(x86_kpg_infra);

    // ! map the bios memory area, should it be done like this?
    pr_info("mapping bios memory area...");
    memblock_t *bios_block = x86_mem_find_bios_block();
    pg_do_map_pages(x86_kpg_infra, bios_block->paddr, bios_block->paddr, bios_block->size_bytes / X86_PAGE_SIZE, VM_GLOBAL);

    x86_mm_enable_paging(x86_kpg_infra);

    mos_kernel_mm_init(); // since then, we can use the kernel heap (kmalloc)

    mos_install_kpanic_hook(x86_kpanic_hook);
    mos_install_console(&vga_text_mode_console);
    x86_install_interrupt_handler(IRQ_COM1, serial_irq_handler);

    if (initrd_memblock.size_bytes > 0)
    {
        initrd_blockdev_t *initrd_blockdev = kmalloc(sizeof(initrd_blockdev_t));
        initrd_blockdev->memblock = initrd_memblock;

        initrd_blockdev_preinstall(initrd_blockdev);
        blockdev_register(&initrd_blockdev->blockdev);
    }

    mos_start_kernel(mos_cmdline);
}

mos_platform_t x86_platform = {
    .regions = {
        .ro_start = (uintptr_t) &__MOS_KERNEL_RO_START,
        .ro_end = (uintptr_t) &__MOS_KERNEL_RO_END,
        .rw_start = (uintptr_t) &__MOS_KERNEL_RW_START,
        .rw_end = (uintptr_t) &__MOS_KERNEL_RW_END,
    },
    .mm_page_size = X86_PAGE_SIZE,

    .halt_cpu = x86_cpu_halt,
    .current_cpu_id = x86_cpu_get_id,

    .shutdown = x86_shutdown_vm,
    .interrupt_disable = x86_disable_interrupts,
    .interrupt_enable = x86_enable_interrupts,
    .irq_handler_install = x86_install_interrupt_handler,
    .irq_handler_remove = NULL,

    // memory management
    .mm_create_pagetable = x86_um_pgd_create,
    .mm_destroy_pagetable = x86_um_pgd_destroy,
    .mm_alloc_pages = x86_mm_pg_alloc,
    .mm_free_pages = x86_mm_pg_free,
    .mm_flag_pages = x86_mm_pg_flag,
    .mm_map_kvaddr = x86_mm_pg_map_to_kvirt,
    .mm_unmap = x86_mm_pg_unmap,

    // process management
    .context_setup = x86_setup_thread_context,
    .switch_to_thread = x86_context_switch,
    .switch_to_scheduler = x86_context_switch_to_scheduler,
};

mos_platform_t *const mos_platform = &x86_platform;
