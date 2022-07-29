// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mos_global.h"
#include "mos/types.h"

#include <stdarg.h>

typedef void(kmsg_handler_t)(const char *func, u32 line, const char *fmt, va_list args);

void kpanic_handler_set(kmsg_handler_t *handler);
void kpanic_handler_remove(void);

void kwarn_handler_set(kmsg_handler_t *handler);
void kwarn_handler_remove(void);
