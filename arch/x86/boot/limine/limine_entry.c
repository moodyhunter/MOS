// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/console.h"
#include "mos/x86/devices/serial_console.h"

extern void x86_start_kernel(void);

asmlinkage void limine_entry(void)
{
    x86_start_kernel();
}
