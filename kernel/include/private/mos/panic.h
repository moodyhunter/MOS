// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/list.h"
#include "mos/types.h"

#include <stdarg.h>

typedef void(kmsg_handler_t)(const char *func, u32 line, const char *fmt, va_list args);
typedef void(kpanic_hook_t)(void);

typedef struct
{
    as_linked_list;
    kpanic_hook_t *hook;
} panic_hook_holder_t;

#define declare_panic_hook(fn) static panic_hook_holder_t fn##_holder = { .hook = fn, .list_node = LIST_NODE_INIT(fn##_holder) }

void install_panic_hook(panic_hook_holder_t *hook);
void kwarn_handler_set(kmsg_handler_t *handler);
void kwarn_handler_remove(void);
