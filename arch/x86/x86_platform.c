// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kernel.h"
#include "mos/mos_global.h"
#include "mos/x86/common.h"
#include "mos/x86/drivers/port.h"
#include "mos/x86/drivers/screen.h"

void x86_disable_interrupts()
{
    __asm__ volatile("cli");
}
void x86_enable_interrupts()
{
    __asm__ volatile("sti");
}

void x86_init()
{
    x86_disable_interrupts();

    x86_gdt_init();
    x86_idt_init();
    x86_tss_init();

    screen_init();
    screen_disable_cursor();
}

void __attr_noreturn x86_shutdown_vm()
{
    x86_disable_interrupts();
    port_outw(0x604, 0x2000);
    while (1)
        ;
}

mos_platform_t mos_platform = {
    .platform_init = x86_init,
    .platform_shutdown = x86_shutdown_vm,
    .disable_interrupts = x86_disable_interrupts,
    .enable_interrupts = x86_enable_interrupts,
};
