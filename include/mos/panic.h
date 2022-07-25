// SPDX-License-Identifier: GPL-3.0-or-later

#include "attributes.h"

#define panic(msg)   _kpanic_impl(msg, __func__, __FILE__, MOS_STRINGIFY(__LINE__))
#define warning(msg) _kwarn_impl(msg, __func__, __FILE__, MOS_STRINGIFY(__LINE__))

typedef void __attr_noreturn (*kpanic_handler_t)(const char *msg, const char *func, const char *file, const char *line);

// TODO: change to appropriate number after printf is implemented
void _kwarn_impl(const char *msg, const char *func, const char *file, const char *line);
__attr_noreturn void _kpanic_impl(const char *msg, const char *func, const char *file, const char *line);

void kpanic_handler_set(kpanic_handler_t handler);
void kpanic_handler_remove(void);
