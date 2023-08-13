// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall_entry.h"

#include "mos/tasks/signal.h"

#include <mos/syscall/dispatcher.h>
#include <mos/types.h>

reg_t ksyscall_enter(reg_t num, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5)
{
    reg_t ret = dispatch_syscall(num, arg0, arg1, arg2, arg3, arg4, arg5);

    return ret;
}
