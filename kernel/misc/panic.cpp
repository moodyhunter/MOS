// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/power.hpp"
#include "mos/syslog/syslog.hpp"

#include <mos/interrupt/ipi.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/misc/cmdline.hpp>
#include <mos/misc/panic.hpp>
#include <mos/misc/setup.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos_stdio.hpp>

// stack smashing protector
u64 __stack_chk_guard = 0;

[[noreturn]] void __stack_chk_fail(void)
{
    mos_panic("Stack smashing detected!");
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

static kmsg_handler_t *kwarn_handler = NULL;
static bool poweroff_on_panic = false;

MOS_EARLY_SETUP("poweroff_on_panic", setup_poweroff_on_panic)
{
    poweroff_on_panic = cmdline_string_truthiness(arg, true);
    return true;
}

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

extern const panic_point_t __MOS_PANIC_LIST_START[], __MOS_PANIC_LIST_END[];
extern const panic_hook_t __MOS_PANIC_HOOKS_START[], __MOS_PANIC_HOOKS_END[];

static const panic_point_t *find_panic_point(ptr_t ip)
{
    const panic_point_t *point = NULL;
    for (const panic_point_t *p = __MOS_PANIC_LIST_START; p < __MOS_PANIC_LIST_END; p++)
    {
        if (p->ip == ip)
        {
            point = p;
            break;
        }
    }
    return point;
}

void try_handle_kernel_panics(ptr_t ip)
{
    const panic_point_t *point = find_panic_point(ip);
    if (!point)
    {
        pr_dwarn(panic, "no panic point found for " PTR_FMT, ip);
        return;
    }
    handle_kernel_panic(point);
}

void handle_kernel_panic(const panic_point_t *point)
{
    platform_interrupt_disable();

    if (!once())
    {
        pr_fatal("recursive panic detected, aborting...");
        pr_info("");
        if (unlikely(poweroff_on_panic))
        {
            pr_emerg("Powering off...");
            power_shutdown();
        }

        while (true)
            ;
    }

    if (printk_unquiet())
        pr_info("quiet mode disabled"); // was quiet

    pr_emerg("");
    pr_fatal("!!!!!!!!!!!!!!!!!!!!!!!!");
    pr_fatal("!!!!! KERNEL PANIC !!!!!");
    pr_fatal("!!!!!!!!!!!!!!!!!!!!!!!!");
    pr_emerg("");
    pr_emerg("file: %s:%llu", point->file, point->line);
    pr_emerg("function: %s", point->func);
    if (point->ip)
        pr_emerg("instruction: %ps (" PTR_FMT ")", (void *) point->ip, point->ip);
    else
        pr_emerg("instruction: see backtrace");
    pr_emerg("");

    pr_cont("\n");

    if (point->ip == 0)
    {
        // inline panic point
        pr_emph("Current stack trace:");
        platform_dump_current_stack();
    }
    else
    {
        if (current_cpu->interrupt_regs)
        {
            pr_emph("Register states before interrupt:");
            platform_dump_regs(current_cpu->interrupt_regs);
            pr_cont("\n");
            pr_emph("Stack trace before interrupt");
            platform_dump_stack(current_cpu->interrupt_regs);
            pr_cont("\n");
        }
        else
        {
            pr_emph("No interrupt context available");
        }
    }

    pr_cont("\n");

    for (const panic_hook_t *hook = __MOS_PANIC_HOOKS_START; hook < __MOS_PANIC_HOOKS_END; hook++)
    {
        if (hook->enabled && !*hook->enabled)
            continue;

        pr_dinfo2(panic, "invoking panic hook '%s' at %ps", hook->name, (void *) hook);
        hook->hook();
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

    char message[MOS_PRINTK_BUFFER_SIZE];
    va_start(args, fmt);
    vsnprintf(message, MOS_PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);

    lprintk(LogLevel::WARN, "\n%s", message);
    lprintk(LogLevel::WARN, "  in function: %s (line %u)\n", func, line);
}
