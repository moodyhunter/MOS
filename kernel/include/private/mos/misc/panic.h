// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform_defs.h"
#include "mos/syslog/printk.h"

#include <mos/lib/structures/list.h>
#include <mos/types.h>
#include <stdarg.h>

typedef void(kmsg_handler_t)(const char *func, u32 line, const char *fmt, va_list args);
typedef void(kpanic_hook_t)(void);

typedef struct
{
    as_linked_list;
    kpanic_hook_t *hook;
    const char *const name;
} panic_hook_holder_t;

#define panic_hook_declare(fn, _name) static panic_hook_holder_t fn##_holder = { .list_node = LIST_NODE_INIT(fn##_holder), .hook = fn, .name = _name }

void panic_hook_install(panic_hook_holder_t *hook);
void kwarn_handler_set(kmsg_handler_t *handler);
void kwarn_handler_remove(void);

__BEGIN_DECLS

__printf(3, 4) void mos_kwarn(const char *func, u32 line, const char *fmt, ...);

void try_handle_kernel_panics(ptr_t ip);

__END_DECLS

#define MOS_MAKE_PANIC_POINT(panic_instruction, file, func, line)                                                                                                        \
    __asm__ volatile("1: " panic_instruction "\n\t"                                                                                                                      \
                     ".pushsection .mos.panic_list,\"aw\"\n\t" MOS_PLATFORM_PANIC_POINT_ASM ".popsection\n\t"                                                            \
                     "\n\t"                                                                                                                                              \
                     :                                                                                                                                                   \
                     : "i"(file), "i"(func), "i"(line)                                                                                                                   \
                     : "memory")

#define mos_panic(fmt, ...)                                                                                                                                              \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        pr_emerg(fmt, ##__VA_ARGS__);                                                                                                                                    \
        MOS_MAKE_PANIC_POINT(MOS_PLATFORM_PANIC_INSTR, __FILE__, __func__, __LINE__);                                                                                    \
        __builtin_unreachable();                                                                                                                                         \
    } while (0)
