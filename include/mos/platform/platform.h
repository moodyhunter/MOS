// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/interrupt.h"
#include "mos/mos_global.h"

typedef struct
{
    const char *cmdline;
} mos_init_info_t;

typedef struct
{
    void *kernel_start;
    void *kernel_end;

    void __noreturn (*shutdown)(void);

    // interrupt
    void (*interrupt_enable)(void);
    void (*interrupt_disable)(void);
    bool (*irq_install_handler)(u32 irq, irq_handler_descriptor_t *handler);
    void (*irq_uninstall_handler)(u32 irq, irq_handler_descriptor_t *handler);

    // memory management
    size_t mm_page_size;
    void (*mm_setup_paging)(void);
    void *(*mm_alloc_page)(size_t n);
    bool (*mm_free_page)(void *ptr, size_t n);
    void (*mm_set_page_flags)(void *ptr, size_t n, u32 flags);
} mos_platform_t;

extern mos_platform_t mos_platform;

extern void mos_start_kernel(mos_init_info_t *init_info);
extern void mos_invoke_syscall(u64 syscall_number);

extern void mos_mem_add_region(u64 start, size_t size, bool available);
extern void mos_mem_finish_setup();
