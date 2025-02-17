// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/misc/setup.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/kthread.hpp>
#include <mos_stdio.hpp>

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
        char namebuf[32];
        snprintf(namebuf, sizeof(namebuf), "idle-%u", i);
        pr_dinfo(process, "creating the idle task for CPU %u", i);
        const auto t = kthread_create_no_sched(idle_task, NULL, namebuf);
        platform_info->cpu.percpu_value[i].idle_thread = t;
        // ! scheduler will switch to this thread if no other threads are available, thus scheduler_add_thread isn't called
        // thread_set_cpu(t, i);
        MOS_UNUSED(t);
    }
}

MOS_INIT(KTHREAD, create_idle_task);
