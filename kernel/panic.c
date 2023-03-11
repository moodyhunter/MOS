// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/panic.h"

#include "lib/stdio.h"
#include "lib/structures/list.h"
#include "mos/cmdline.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/setup.h"

// stack smashing protector
u64 __stack_chk_guard = 0;

noreturn void __stack_chk_fail(void)
{
    mos_panic("Stack smashing detected!");
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

static list_head kpanic_hooks = LIST_HEAD_INIT(kpanic_hooks);
static kmsg_handler_t *kwarn_handler = NULL;
static bool poweroff_on_panic = false;

static bool setup_poweroff_on_panic(int argc, const char **argv)
{
    bool val = cmdline_arg_get_bool(argc, argv, false);
    poweroff_on_panic = val;
    return true;
}
__setup("poweroff_on_panic", setup_poweroff_on_panic);

void kwarn_handler_set(kmsg_handler_t *handler)
{
    pr_warn("installing a new warning handler...");
    kwarn_handler = handler;
}

void kwarn_handler_remove(void)
{
    pr_warn("removing warning handler...");
    if (!kwarn_handler)
        mos_warn("no previous warning handler installed");
    kwarn_handler = NULL;
}

noreturn void mos_kpanic(const char *func, u32 line, const char *fmt, ...)
{
    static bool in_panic = false;
    if (unlikely(in_panic))
    {
        pr_fatal("recursive panic detected, aborting...");
        if (unlikely(poweroff_on_panic))
        {
            pr_emerg("Powering off...");
            platform_shutdown();
        }

        while (true)
            ;
    }
    in_panic = true;
    platform_interrupt_disable();

    extern bool printk_quiet;
    printk_quiet = false; // make sure we print the panic message
    pr_info("quiet mode disabled, printing panic message...");

    va_list args;
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

    list_foreach(panic_hook_holder_t, holder, kpanic_hooks)
    {
        holder->hook();
    }

    if (unlikely(poweroff_on_panic))
    {
        pr_emerg("Powering off...");
        platform_shutdown();
    }

    pr_emerg("Halting...");
    platform_halt_cpu();

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

    lprintk(MOS_LOG_WARN, "%s", message);
    lprintk(MOS_LOG_WARN, "  in function: %s (line %u)\n", func, line);
}

void install_panic_hook(panic_hook_holder_t *hook)
{
    list_node_append(&kpanic_hooks, list_node(hook));
}
