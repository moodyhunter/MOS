// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/boot/multiboot.h"
#include "mos/drivers/screen.h"
#include "mos/gdt.h"
#include "mos/idt.h"
#include "mos/kconfig.h"
#include "mos/kernel.h"
#include "mos/stdio.h"

#ifdef MOS_KERNEL_RUN_TESTS
extern void test_engine_run_tests();
#endif

void start_kernel(u32 magic, multiboot_info_t *addr)
{
    gdt_init();

    // !! IDT IS BROKEN AT THE MOMENT
    // idt_init();

    screen_init();
    screen_disable_cursor();

    pr_info("Welcome to MOS!");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        mos_panic("invalid magic number: %x", magic);

    if (!(addr->flags & MULTIBOOT_INFO_MEM_MAP))
        mos_panic("no memory map");

    pr_info("MOS Information:");
    pr_emph("cmdline: %s", addr->cmdline);
    pr_emph("%-25s'%s'", "Kernel Version:", MOS_KERNEL_VERSION);
    pr_emph("%-25s'%s'", "Kernel Revision:", MOS_KERNEL_REVISION);
    pr_emph("%-25s'%s'", "Kernel builtin cmdline:", MOS_KERNEL_BUILTIN_CMDLINE);

    mos_warn("V2Ray 4.45.2 started");

#ifdef MOS_KERNEL_RUN_TESTS
    test_engine_run_tests();
#endif
    while (1)
        ;
}
