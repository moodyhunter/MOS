// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/panic.h"

#include "mos/kernel.h"
#include "mos/stdio.h"

#include <stdarg.h>

static kmsg_handler_t *kpanic_handler = NULL;
static kmsg_handler_t *kwarn_handler = NULL;

void kwarn_handler_set(kmsg_handler_t *handler)
{
    pr_warn("installing a new warning handler...");
    kwarn_handler = handler;
}

void kpanic_handler_set(kmsg_handler_t *handler)
{
    pr_warn("installing a new panic handler...");
    kpanic_handler = handler;
}

void kwarn_handler_remove()
{
    pr_warn("removing warning handler...");
    if (!kwarn_handler)
        mos_warn("no previous warning handler installed");
    kwarn_handler = NULL;
}

void kpanic_handler_remove()
{
    pr_warn("removing panic handler...");
    if (!kpanic_handler)
        mos_warn("no previous panic handler installed");
    kpanic_handler = NULL;
}

// ! using the following 2 functions causes recurstion
#undef mos_warn
#undef mos_panic

void mos_kpanic(const char *func, u32 line, const char *fmt, ...)
{
    mos_platform.disable_interrupts();

    va_list args;
    if (kpanic_handler)
    {
        va_start(args, fmt);
        (*kpanic_handler)(func, line, fmt, args);
        va_end(args);
        pr_fatal("kpanic handler returned!");
        while (1)
            ;
    }

    char message[PRINTK_BUFFER_SIZE] = { 0 };
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);

    pr_emerg("");
    pr_fatal("!!!!!!!!!!!!!!!!!!!!!!!!");
    pr_fatal("!!!!! KERNEL PANIC !!!!!");
    pr_fatal("!!!!!!!!!!!!!!!!!!!!!!!!");
    pr_emerg("");
    pr_emerg("%s", message);
    pr_emerg("  in function: %s (line %u)", func, line);

    while (1)
        ;
}

void mos_kwarn(const char *func, u32 line, const char *fmt, ...)
{
    va_list args;
    if (kwarn_handler)
    {
        va_start(args, fmt);
        kwarn_handler(func, line, fmt, args);
        va_end(args);
        return;
    }

    char message[PRINTK_BUFFER_SIZE] = { 0 };
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);

    pr_warn("warning: %s", message);
    pr_warn("  in function: %s (line %u)", func, line);
}
