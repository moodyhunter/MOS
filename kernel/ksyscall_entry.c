// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall_entry.h"

#include "mos/tasks/signal.h"

#include <mos/syscall/dispatcher.h>
#include <mos/types.h>

reg_t ksyscall_enter(reg_t num, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5)
{
    reg_t ret = dispatch_syscall(num, arg0, arg1, arg2, arg3, arg4, arg5);
    if (IS_ERR_VALUE(ret))
    {
        // handle -EFAULT by sending SIGSEGV to the current thread
        if (ret == (reg_t) -EFAULT)
        {
            signal_send_to_thread(current_thread, SIGSEGV);
            signal_check_and_handle();
        }
    }
    MOS_ASSERT_X(current_thread->state == THREAD_STATE_RUNNING, "thread %pt is not in 'running' state", (void *) current_thread);
    return ret;
}
