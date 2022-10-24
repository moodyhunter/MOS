// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "lib/string.h"
#include "mos/constants.h"
#include "mos/kconfig.h"
#include "mos/mm/kmalloc.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/types.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/smp.h"
#include "mos/x86/devices/initrd_blockdev.h"
#include "mos/x86/devices/serial_console.h"
#include "mos/x86/devices/text_mode_console.h"
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

void x86_start_kernel(x86_startup_info *info)
{
    multiboot_info_t *mb_info = info->mb_info;
    uintptr_t initrd_size = info->initrd_size;
    x86_disable_interrupts();
    mos_install_console(&com1_console.console);

    x86_gdt_init();
    x86_idt_init();
    x86_tss_init();
    x86_irq_handler_init();

    // I don't like the timer interrupt, so disable it.
    pic_unmask_irq(IRQ_TIMER);
    pic_unmask_irq(IRQ_KEYBOARD);
    pic_unmask_irq(IRQ_COM1);
    pic_unmask_irq(IRQ_COM2);

    if (mb_info->flags & MULTIBOOT_INFO_CMDLINE)
        strncpy(mos_cmdline, mb_info->cmdline, sizeof(mos_cmdline));

    u32 count = mb_info->mmap_length / sizeof(multiboot_memory_map_t);
    x86_mem_init(mb_info->mmap_addr, count);

    x86_mm_prepare_paging();
    x86_platform.kernel_pg.ptr = (uintptr_t) x86_kpg_infra;

    if (initrd_size)
    {
        reg_t cr3 = x86_get_cr3();
        const uintptr_t initrd_vaddr = MOS_X86_INITRD_VADDR;
        x86_pgtable_entry *table = (x86_pgtable_entry *) (((x86_pgdir_entry *) cr3)[initrd_vaddr >> 22].page_table_paddr << 12);
        const uintptr_t initrd_paddr = table->phys_addr << 12;
        const size_t initrd_n_pages = ALIGN_UP_TO_PAGE(initrd_size) / MOS_PAGE_SIZE;
        pg_do_map_pages(x86_kpg_infra, initrd_vaddr, initrd_paddr, initrd_n_pages, VM_GLOBAL);
    }

    x86_acpi_init();
    x86_smp_init();

    // ! map the bios memory area, should it be done like this?
    pr_info("mapping bios memory area...");
    for (u32 i = 0; i < x86_mem_regions_count; i++)
        if (x86_mem_regions[i].paddr == info->bios_region_start)
            x86_bios_region = &x86_mem_regions[i];

    memblock_t *bios_block = x86_bios_region;
    pg_do_map_pages(x86_kpg_infra, bios_block->paddr, bios_block->paddr, bios_block->size_bytes / MOS_PAGE_SIZE, VM_GLOBAL);

#if MOS_DEBUG
    x86_mm_dump_page_table(x86_kpg_infra);
    pmem_freelist_dump();
#endif

    x86_mm_enable_paging(x86_kpg_infra);

    mos_kernel_mm_init(); // since then, we can use the kernel heap (kmalloc)

    mos_install_kpanic_hook(x86_kpanic_hook);
    mos_install_console(&vga_text_mode_console);
    x86_install_interrupt_handler(IRQ_COM1, serial_irq_handler);

    if (initrd_size)
    {
        initrd_blockdev_t *initrd_blockdev = kmalloc(sizeof(initrd_blockdev_t));
        initrd_blockdev->memblock = (memblock_t){ .available = true, .vaddr = MOS_X86_INITRD_VADDR, .size_bytes = initrd_size };

        initrd_blockdev_preinstall(initrd_blockdev);
        blockdev_register(&initrd_blockdev->blockdev);
    }

    mos_start_kernel(mos_cmdline);
}

mos_platform_t x86_platform = {
    .regions = {
        .code_start = (uintptr_t) &__MOS_KERNEL_CODE_START,
        .code_end = (uintptr_t) &__MOS_KERNEL_CODE_END,
        .rodata_start = (uintptr_t) &__MOS_KERNEL_RODATA_START,
        .rodata_end = (uintptr_t) &__MOS_KERNEL_RODATA_END,
        .rw_start = (uintptr_t) &__MOS_KERNEL_RW_START,
        .rw_end = (uintptr_t) &__MOS_KERNEL_RW_END,
    },

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
    // .mm_flag_pages = x86_mm_pg_flag,
    .mm_map_kvaddr = x86_mm_pg_map_to_kvirt,
    .mm_unmap = x86_mm_pg_unmap,

    // process management
    .context_setup = x86_setup_thread_context,
    .switch_to_thread = x86_context_switch,
    .switch_to_scheduler = x86_context_switch_to_scheduler,
};

mos_platform_t *const mos_platform = &x86_platform;
