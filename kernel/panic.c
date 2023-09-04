// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/console.h"
#include "mos/misc/power.h"

#include <mos/cmdline.h>
#include <mos/interrupt/ipi.h>
#include <mos/lib/structures/list.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/setup.h>
#include <mos_stdio.h>

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

static bool setup_poweroff_on_panic(const char *arg)
{
    poweroff_on_panic = cmdline_string_truthiness(arg, true);
    return true;
}
MOS_EARLY_SETUP("poweroff_on_panic", setup_poweroff_on_panic);

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
    platform_interrupt_disable();

    // unlock the consoles, in case we were in the middle of writing something
    list_foreach(console_t, console, consoles)
    {
        spinlock_release(&console->write.lock);
    }

    static bool in_panic = false;
    if (unlikely(in_panic))
    {
        pr_fatal("recursive panic detected, aborting...");
        if (unlikely(poweroff_on_panic))
        {
            pr_emerg("Powering off...");
            power_shutdown();
        }

        while (true)
            ;
    }
    in_panic = true;

    extern bool printk_quiet;
    if (printk_quiet)
    {
        pr_info("quiet mode disabled, printing panic message...");
        printk_quiet = false; // make sure we print the panic message
    }

    va_list args;
    char message[PRINTK_BUFFER_SIZE];
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
        mos_debug(panic, "invoking panic hook '%s' at %ps", holder->name, (void *) holder);
        holder->hook();
    }

    ipi_send_all(IPI_TYPE_HALT);

    if (unlikely(poweroff_on_panic))
    {
        pr_emerg("Powering off...");
        power_shutdown();
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

    char message[PRINTK_BUFFER_SIZE];
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);

    lprintk(MOS_LOG_WARN, "\n%s", message);
    lprintk(MOS_LOG_WARN, "  in function: %s (line %u)\n", func, line);
}

void panic_hook_install(panic_hook_holder_t *hook)
{
    list_node_append(&kpanic_hooks, list_node(hook));
    mos_debug(panic, "installed panic hook '%s' at %ps", hook->name, (void *) (ptr_t) hook->hook);
}
