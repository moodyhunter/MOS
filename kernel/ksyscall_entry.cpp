// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall_entry.hpp"

#include "mos/misc/profiling.hpp"
#include "mos/tasks/signal.hpp"

#include <mos/syscall/dispatcher.h>
#include <mos/syscall/table.h>
#include <mos/types.hpp>
#include <mos_stdio.hpp>

reg_t ksyscall_enter(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5, reg_t arg6)
{
    const pf_point_t ev = profile_enter();
    const reg_t ret = dispatch_syscall(number, arg1, arg2, arg3, arg4, arg5, arg6);
    profile_leave(ev, "syscall.%lu.%s", number, get_syscall_names(number));

    if (IS_ERR_VALUE(ret))
    {
        // handle -EFAULT by sending SIGSEGV to the current thread
        if (ret == (reg_t) -EFAULT)
            signal_send_to_thread(current_thread, SIGSEGV);
    }

    MOS_ASSERT_X(current_thread->state == THREAD_STATE_RUNNING, "thread %pt is not in 'running' state", current_thread);
    return ret;
}
