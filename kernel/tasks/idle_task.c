// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/setup.h>
#include <mos/tasks/kthread.h>

static void idle_task(void *arg)
{
    MOS_UNUSED(arg);
    platform_interrupt_enable();
    while (true)
        platform_cpu_idle();
}

void create_idle_task()
{
    pr_info2("creating the idle task...");
    kthread_create(idle_task, NULL, "idle");
}

MOS_INIT(KTHREAD, create_idle_task);
