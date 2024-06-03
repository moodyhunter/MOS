// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/misc/setup.h>
#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
#include <mos/tasks/kthread.h>

static void idle_task(void *arg)
{
    MOS_UNUSED(arg);
    platform_interrupt_enable();
    while (true)
        platform_cpu_idle();
}

static void create_idle_task()
{
    pr_dinfo2(process, "creating the idle task...");

    for (u32 i = 0; i < platform_info->num_cpus; i++)
    {
        pr_dinfo(process, "creating the idle task for CPU %u", i);
        thread_t *t = kthread_create(idle_task, NULL, "idle");
        // thread_set_cpu(t, i);
        MOS_UNUSED(t);
    }
}

MOS_INIT(KTHREAD, create_idle_task);
