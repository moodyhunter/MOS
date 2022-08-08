// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

typedef struct
{
    const char *cmdline;
} mos_init_info_t;

typedef enum
{
    PAGING_NONE = 0,
    PAGING_PRESENT = 1 << 0,
    PAGING_WRITABLE = 1 << 1,
    PAGING_USERMODE = 1 << 2,
    PAGING_WRITE_THROUGH = 1 << 3,
    PAGING_CACHE_DISABLED = 1 << 4,
    PAGING_ACCESSED = 1 << 5,
} paging_entry_flags;

typedef struct
{
    const void *kernel_start;
    const void *kernel_end;

    void __noreturn (*shutdown)(void);
    void (*devices_setup)(mos_init_info_t *init_info);

    // interrupt
    void (*interrupt_enable)(void);
    void (*interrupt_disable)(void);
    bool (*irq_handler_install)(u32 irq, void (*handler)(u32 irq));
    void (*irq_handler_remove)(u32 irq, void (*handler)(u32 irq));

    // memory management
    size_t mm_page_size;
    void *(*mm_page_allocate)(size_t n);
    bool (*mm_page_free)(void *vaddr, size_t n);
    void (*mm_page_set_flags)(void *vaddr, size_t n, paging_entry_flags flags);
} mos_platform_t;

extern const mos_platform_t mos_platform;

extern void mos_start_kernel(mos_init_info_t *init_info);
extern void mos_invoke_syscall(u64 syscall_number);
