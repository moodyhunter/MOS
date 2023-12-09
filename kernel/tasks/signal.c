// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/signal.h"

#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/thread.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/tasks/signal_types.h>
#include <mos_stdlib.h>

slab_t *sigpending_slab = NULL;
SLAB_AUTOINIT("signal_pending", sigpending_slab, sigpending_t);

noreturn static void signal_do_coredump(signal_t signal)
{
    if (current_thread == current_process->main_thread)
        process_handle_exit(current_process, 0, signal);
    else
        thread_handle_exit(current_thread);
    MOS_UNREACHABLE();
}

noreturn static void signal_do_terminate(signal_t signal)
{
    if (current_thread == current_process->main_thread)
        process_handle_exit(current_process, 0, signal);
    else
        thread_handle_exit(current_thread);
    MOS_UNREACHABLE();
}

static void signal_do_ignore(signal_t signal)
{
    pr_dinfo2(signal, "thread %pt ignoring signal %d", (void *) current_thread, signal);
}

long signal_send_to_thread(thread_t *target, signal_t signal)
{
    if (target->mode == THREAD_MODE_KERNEL)
    {
        pr_emerg("signal_send_to_thread(%pt, %d): cannot send signal to kernel thread", (void *) target, signal);
        return -EINVAL;
    }

    spinlock_acquire(&target->signal_info.lock);

    bool has_pending = false; // true if the signal is already pending
    list_foreach(sigpending_t, pending, target->signal_info.pending)
    {
        has_pending |= pending->signal == signal;
        if (has_pending)
            break;
    }

    if (!has_pending)
    {
        sigpending_t *sigdesc = kmalloc(sigpending_slab);
        linked_list_init(list_node(sigdesc));
        sigdesc->signal = signal;
        list_node_append(&target->signal_info.pending, list_node(sigdesc));
    }

    spinlock_release(&target->signal_info.lock);
    return 0;
}

long signal_send_to_process(process_t *target, signal_t signal)
{
    if (target->pid == 1 && signal == SIGKILL)
    {
        pr_emerg("signal_send_to_process(%pp, %d): cannot send SIGKILL to init", (void *) target, signal);
        return -EINVAL;
    }

    if (target->pid == 2)
    {
        pr_emerg("signal_send_to_process(%pp, %d): cannot send signal to kthreadd", (void *) target, signal);
        return -EINVAL;
    }

    thread_t *target_thread = NULL;
    list_foreach(thread_t, thread, target->threads)
    {
        if (thread->state == THREAD_STATE_RUNNING || thread->state == THREAD_STATE_READY || thread->state == THREAD_STATE_CREATED)
        {
            target_thread = thread;
            break;
        }
    }

    if (!target_thread)
    {
        list_foreach(thread_t, thread, target->threads)
        {
            if (thread->state == THREAD_STATE_BLOCKED)
            {
                target_thread = thread;
                break;
            }
        }
    }

    if (!target_thread)
    {
        pr_emerg("signal_send_to_process(%pp, %d): no thread to send signal to", (void *) target, signal);
        return -EINVAL;
    }

    signal_send_to_thread(target_thread, signal);

    if (target_thread != current_thread)
    {
        spinlock_acquire(&target_thread->state_lock);
        if (target_thread->state == THREAD_STATE_BLOCKED)
            target_thread->state = THREAD_STATE_READY;
        spinlock_release(&target_thread->state_lock);
    }

    return 0;
}

static signal_t signal_get_next_pending(void)
{
    signal_t signal = 0;

    MOS_ASSERT(spinlock_is_locked(&current_thread->signal_info.lock));
    list_foreach(sigpending_t, pending, current_thread->signal_info.pending)
    {
        if (current_thread->signal_info.masks[pending->signal])
            continue; // signal is masked, skip it

        list_remove(pending);
        signal = pending->signal;
        kfree(pending);
        break;
    }

    return signal;
}

static void do_signal_exit_to_user_prepare(platform_regs_t *regs, signal_t next_signal, const sigaction_t *action)
{
    if (action->sa_handler == SIG_DFL)
    {
        if (current_process->pid == 1)
            goto done; // init only receives signals it wants

        switch (next_signal)
        {
            case SIGINT: signal_do_terminate(next_signal); break;
            case SIGILL: signal_do_coredump(next_signal); break;
            case SIGTRAP: signal_do_coredump(next_signal); break;
            case SIGABRT: signal_do_coredump(next_signal); break;
            case SIGKILL: signal_do_terminate(next_signal); break;
            case SIGSEGV: signal_do_coredump(next_signal); break;
            case SIGTERM: signal_do_terminate(next_signal); break;
            case SIGCHLD: signal_do_ignore(next_signal); break;
            case SIGPIPE: signal_do_terminate(next_signal); break;

            default: MOS_UNREACHABLE_X("handle this signal %d", next_signal); break;
        }

    // the default handler returns
    done:
        return;
    }

    if (action->sa_handler == SIG_IGN)
    {
        signal_do_ignore(next_signal);
        return;
    }

    const bool was_masked = current_thread->signal_info.masks[next_signal];
    if (!was_masked)
        current_thread->signal_info.masks[next_signal] = true;

    const sigreturn_data_t data = {
        .signal = next_signal,
        .was_masked = was_masked,
    };

    platform_jump_to_signal_handler(regs, &data, action); // save previous register states onto user stack
}

void signal_exit_to_user_prepare(platform_regs_t *regs)
{
    MOS_ASSERT(current_thread);

    spinlock_acquire(&current_thread->signal_info.lock);
    const signal_t next_signal = signal_get_next_pending();
    spinlock_release(&current_thread->signal_info.lock);

    if (!next_signal)
        return; // no pending signal, leave asap

    const sigaction_t action = current_process->signal_info.handlers[next_signal];

    do_signal_exit_to_user_prepare(regs, next_signal, &action);
}

void signal_exit_to_user_prepare_syscall(platform_regs_t *regs, reg_t syscall_nr, reg_t syscall_ret)
{
    MOS_ASSERT(current_thread);

    spinlock_acquire(&current_thread->signal_info.lock);
    const signal_t next_signal = signal_get_next_pending();
    spinlock_release(&current_thread->signal_info.lock);

    const sigaction_t action = current_process->signal_info.handlers[next_signal];

    reg_t real_ret = syscall_ret;
    if (syscall_ret == (reg_t) -ERESTARTSYS)
    {
        MOS_ASSERT(next_signal);
        real_ret = -EINTR;

        if (action.sa_flags & SA_RESTART)
        {
            pr_dinfo2(signal, "thread %pt will restart syscall %lu after signal %d", (void *) current_thread, syscall_nr, next_signal);
            platform_syscall_restart(regs, syscall_nr);
            goto really_prepare;
        }
        // else: fall through, return -EINTR
    }

    platform_syscall_result(regs, real_ret);

    if (!next_signal)
        return; // no pending signal, leave asap

really_prepare:
    do_signal_exit_to_user_prepare(regs, next_signal, &action);
}

void signal_on_returned(sigreturn_data_t *data)
{
    if (!data->was_masked)
        current_thread->signal_info.masks[data->signal] = false;
}

bool signal_has_pending(void)
{
    spinlock_acquire(&current_thread->signal_info.lock);
    const bool has_pending = !list_is_empty(&current_thread->signal_info.pending);
    spinlock_release(&current_thread->signal_info.lock);
    return has_pending;
}
