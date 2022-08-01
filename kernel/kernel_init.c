// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/device/console.h"
#include "mos/kconfig.h"
#include "mos/platform.h"
#include "mos/printk.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/drivers/serial.h"
#include "mos/x86/x86_platform.h"

#ifdef MOS_KERNEL_RUN_TESTS
extern void mos_test_engine_run_tests();
#endif

u64 __stack_chk_guard = 0;

void __stack_chk_fail()
{
    mos_panic("Stack smashing detected!");
    while (1)
        ;
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

void start_kernel(u32 magic, multiboot_info_t *addr)
{
    mos_platform.platform_init();
    mos_platform.enable_interrupts();

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
    mos_test_engine_run_tests();
#endif
    while (1)
        ;
}
