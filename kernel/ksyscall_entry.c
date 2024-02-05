// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall_entry.h"

#include "mos/misc/profiling.h"
#include "mos/syscall/table.h"
#include "mos/tasks/signal.h"

#include <mos/syscall/dispatcher.h>
#include <mos/types.h>
#include <mos_stdio.h>

reg_t ksyscall_enter(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5, reg_t arg6)
{
    const pf_point_t ev = profile_enter();
    reg_t ret = dispatch_syscall(number, arg1, arg2, arg3, arg4, arg5, arg6);
    profile_leave(ev, "syscall.%lu.%s", num, syscall_names[num]);

    if (IS_ERR_VALUE(ret))
    {
        // handle -EFAULT by sending SIGSEGV to the current thread
        if (ret == (reg_t) -EFAULT)
            signal_send_to_thread(current_thread, SIGSEGV);
    }

    MOS_ASSERT_X(current_thread->state == THREAD_STATE_RUNNING, "thread %pt is not in 'running' state", (void *) current_thread);
    return ret;
}
