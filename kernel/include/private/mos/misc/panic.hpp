// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform_defs.hpp"
#include "mos/syslog/printk.hpp"

#include <mos/types.h>
#include <stdarg.h>

typedef void(kmsg_handler_t)(const char *func, u32 line, const char *fmt, va_list args);

typedef struct
{
    bool *enabled;
    void (*hook)(void);
    const char *const name;
    long long __padding;
} panic_hook_t;

MOS_STATIC_ASSERT(sizeof(panic_hook_t) == 32, "panic_hook_t size mismatch");

typedef struct
{
    ptr_t ip;
    const char *file, *func;
    u64 line;
} panic_point_t;

#define MOS_EMIT_PANIC_HOOK(e, f, n) MOS_PUT_IN_SECTION(".mos.panic_hooks", panic_hook_t, f##_hook, { .enabled = e, .hook = f, .name = n })

#define MOS_PANIC_HOOK_FEAT(_feat, _f, _n) MOS_EMIT_PANIC_HOOK(mos_debug_enabled_ptr(_feat), _f, _n)
#define MOS_PANIC_HOOK(_f, _name)          MOS_EMIT_PANIC_HOOK(NULL, _f, _name)

void kwarn_handler_set(kmsg_handler_t *handler);
void kwarn_handler_remove(void);

__printf(3, 4) void mos_kwarn(const char *func, u32 line, const char *fmt, ...);

void try_handle_kernel_panics(ptr_t ip);
[[__noreturn__]] void handle_kernel_panic(const panic_point_t *point);

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

#define mos_panic_inline(fmt, ...)                                                                                                                                       \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        pr_emerg(fmt, ##__VA_ARGS__);                                                                                                                                    \
        static const panic_point_t point = { .ip = 0, .file = __FILE__, .func = __func__, .line = __LINE__ };                                                            \
        handle_kernel_panic(&point);                                                                                                                                     \
    } while (0)
