// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/types.h"

#include <stdarg.h>

typedef void(kmsg_handler_t)(const char *func, u32 line, const char *fmt, va_list args);
typedef void(kpanic_hook_t)(void);

void mos_install_kpanic_hook(kpanic_hook_t *hook);
void kwarn_handler_set(kmsg_handler_t *handler);
void kwarn_handler_remove(void);
