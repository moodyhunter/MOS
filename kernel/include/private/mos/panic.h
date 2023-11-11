// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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

__printf(3, 4) void mos_kwarn(const char *func, u32 line, const char *fmt, ...);
noreturn __printf(3, 4) void mos_kpanic(const char *func, u32 line, const char *fmt, ...);
