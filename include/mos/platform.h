// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/interrupt.h"
#include "mos/mos_global.h"

typedef struct
{
    void (*platform_init)(void);
    void __noreturn (*platform_shutdown)(void);
    void (*enable_interrupts)(void);
    void (*disable_interrupts)(void);
    void (*install_irq_handler)(u32 irq, irq_handler_descriptor_t *handler);
} mos_platform_t;

extern mos_platform_t mos_platform;
