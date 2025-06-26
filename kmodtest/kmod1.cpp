// SPDX-License-Identifier: GPL-3.0-or-later

#include "header.hpp"
#include "mos/device/timer.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/syslog/syslog.hpp"
#include "mos/tasks/kthread.hpp"

#include <mos/kmod/kmod-decl.hpp>
#include <mos/string.hpp>

mos::string str = "ABCDE";

static void thread_entry(void *)
{
    for (int i = 0; i < 10; i++)
    {
        mInfo << "Thread running: " << i;
        timer_msleep(1000);
    }
}

void kmodentry()
{
    kthread_create(thread_entry, NULL, "thread");
}

KMOD_ENTRYPOINT(kmodentry);
KMOD_NAME("kmod1");
KMOD_AUTHOR("Mos Kernel Developers");
KMOD_DESCRIPTION("Test kernel module 1 for MOS Kernel");
