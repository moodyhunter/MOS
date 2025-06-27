// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/signal.hpp"

#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/process.hpp"
#include "mos/tasks/schedule.hpp"
#include "mos/tasks/thread.hpp"

#include <errno.h>
#include <limits.h>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/syscall/number.h>
#include <mos/tasks/signal_types.h>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

static int sigset_add(sigset_t *sigset, int sig)
{
    MOS_ASSERT(sig >= 1 && sig <= SIGNAL_MAX_N);

    const int signo = sig - 1;
    char *const ptr = (char *) sigset;
    ptr[signo / CHAR_BIT] |= (1 << (signo % CHAR_BIT));
    return 0;
}

static int sigset_del(sigset_t *sigset, int sig)
{
    MOS_ASSERT(sig >= 1 && sig <= SIGNAL_MAX_N);

    const int signo = sig - 1;
    char *const ptr = (char *) sigset;
    ptr[signo / CHAR_BIT] &= ~(1 << (signo % CHAR_BIT));
    return 0;
}

static int sigset_test(const sigset_t *sigset, int sig)
{
    MOS_ASSERT(sig >= 1 && sig <= SIGNAL_MAX_N);

    const int signo = sig - 1;
    const char *ptr = (const char *) sigset;
    return (ptr[signo / CHAR_BIT] & (1 << (signo % CHAR_BIT))) != 0;
}

[[noreturn]] static void signal_do_coredump(signal_t signal)
{
    process_exit(std::move(current_process), 0, signal);
    MOS_UNREACHABLE();
}

[[noreturn]] static void signal_do_terminate(signal_t signal)
{
    if (current_thread == current_process->main_thread)
        process_exit(std::move(current_process), 0, signal);
    else
        thread_exit(std::move(current_thread));
    MOS_UNREACHABLE();
}

static void signal_do_ignore(signal_t signal)
{
    pr_dinfo2(signal, "thread %pt ignoring signal %d", current_thread, signal);
}

static bool is_fatal_signal(signal_t signal)
{
    switch (signal)
    {
        case SIGILL:
        case SIGTRAP:
        case SIGABRT:
        case SIGKILL:
        case SIGSEGV: return true;

        case SIGINT:
        case SIGTERM:
        case SIGCHLD:
        case SIGPIPE: return false;

        default: pr_emerg("is_fatal_signal(%d): returning true", signal); return true;
    }
}

long signal_send_to_thread(Thread *target, signal_t signal)
{
    if (target->mode == THREAD_MODE_KERNEL && !is_fatal_signal(signal))
    {
        pr_emerg("signal_send_to_thread(%pt, %d): cannot send non-fatal signal to kernel thread", target, signal);
        return -EINVAL;
    }

    spinlock_acquire(&target->signal_info.lock);

    bool has_pending = false; // true if the signal is already pending
    for (const auto pending : target->signal_info.pending)
    {
        has_pending |= pending == signal;
        if (has_pending)
            break;
    }

    if (!has_pending)
        target->signal_info.pending.push_back(signal);

    spinlock_release(&target->signal_info.lock);

    if (target != current_thread)
        scheduler_wake_thread(target);

    return 0;
}

long signal_send_to_process(Process *target, signal_t signal)
{
    if (target->pid == 1 && signal == SIGKILL)
    {
        pr_emerg("signal_send_to_process(%pp, %d): cannot send SIGKILL to init", target, signal);
        return -EINVAL;
    }

    if (target->pid == 2)
    {
        pr_emerg("signal_send_to_process(%pp, %d): cannot send signal to kthreadd", target, signal);
        return -EINVAL;
    }

    Thread *target_thread = NULL;
    for (const auto &thread : target->thread_list)
    {
        if (thread->state == THREAD_STATE_RUNNING || thread->state == THREAD_STATE_READY || thread->state == THREAD_STATE_CREATED)
        {
            target_thread = thread;
            break;
        }
    }

    if (!target_thread)
    {
        for (const auto &thread : target->thread_list)
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
        pr_emerg("signal_send_to_process(%pp, %d): no thread to send signal to", target, signal);
        return -EINVAL;
    }

    signal_send_to_thread(target_thread, signal);

    return 0;
}

static signal_t signal_get_next_pending(void)
{
    signal_t signal = 0;

    MOS_ASSERT(spinlock_is_locked(&current_thread->signal_info.lock));
    for (auto it = current_thread->signal_info.pending.begin(); it != current_thread->signal_info.pending.end();)
    {
        signal_t pending = *it;
        if (sigset_test(&current_thread->signal_info.mask, pending))
        {
            // if a fatal signal is pending but also masked, kill the thread
            if (is_fatal_signal(pending))
            {
                pr_emerg("thread %pt received fatal signal %d but it was masked, terminating", current_thread, pending);
                signal_do_terminate(pending);
            }
            it++;
        }
        else
        {
            // signal is not masked, remove it from pending list
            it = current_thread->signal_info.pending.erase(it);
            pr_dinfo2(signal, "thread %pt will handle pending signal %d", current_thread, pending);
            signal = pending;
            break;
        }
    }

    return signal;
}

static ptr<platform_regs_t> do_signal_exit_to_user_prepare(platform_regs_t *regs, signal_t next_signal, const sigaction_t *action)
{
    if (action->handler == SIG_DFL)
    {
        if (current_process->pid == 1 && !is_fatal_signal(next_signal))
            goto done; // init only receives signals it wants

        switch (next_signal)
        {
            case SIGINT: signal_do_terminate(next_signal); break;
            case SIGILL: signal_do_coredump(next_signal); break;
            case SIGTRAP: signal_do_coredump(next_signal); break;
            case SIGABRT: signal_do_coredump(next_signal); break;
            case SIGKILL: signal_do_terminate(next_signal); break;
            case SIGSEGV: signal_do_coredump(next_signal); break;
            case SIGPIPE: signal_do_terminate(next_signal); break;
            case SIGTERM: signal_do_terminate(next_signal); break;
            case SIGCHLD: signal_do_ignore(next_signal); break;
            default: pr_emerg("unknown signal %d, skipping", next_signal); break;
        }

    // the default handler returns
    done:
        return nullptr;
    }

    if (action->handler == SIG_IGN)
    {
        signal_do_ignore(next_signal);
        return nullptr;
    }

    const bool was_masked = sigset_test(&current_thread->signal_info.mask, next_signal);
    if (!was_masked)
        sigset_add(&current_thread->signal_info.mask, next_signal);

    const sigreturn_data_t data = {
        .signal = next_signal,
        .was_masked = was_masked,
    };

    return platform_setup_signal_handler_regs(regs, &data, action); // save previous register states onto user stack
}

ptr<platform_regs_t> signal_exit_to_user_prepare(platform_regs_t *regs)
{
    MOS_ASSERT(current_thread);

    spinlock_acquire(&current_thread->signal_info.lock);
    const signal_t next_signal = signal_get_next_pending();
    spinlock_release(&current_thread->signal_info.lock);

    if (!next_signal)
        return nullptr; // no pending signal, leave asap

    const sigaction_t action = current_process->signal_info.handlers[next_signal];

    pr_dinfo2(signal, "thread %pt will handle signal %d with handler %p", current_thread, next_signal, action.handler);
    return do_signal_exit_to_user_prepare(regs, next_signal, &action);
}

ptr<platform_regs_t> signal_exit_to_user_prepare(platform_regs_t *regs, reg_t syscall_nr, reg_t syscall_ret)
{
    MOS_ASSERT(current_thread);

    spinlock_acquire(&current_thread->signal_info.lock);
    const signal_t next_signal = signal_get_next_pending();
    spinlock_release(&current_thread->signal_info.lock);

    const sigaction_t action = current_process->signal_info.handlers[next_signal];

    // HACK: return if new thread (platform_setup_thread clears a7)
    if (syscall_nr == 0 && syscall_ret == 0)
        return nullptr;

    if (syscall_ret == (reg_t) -ERESTARTSYS)
    {
        MOS_ASSERT(next_signal);

        if (action.sa_flags & SA_RESTART)
        {
            pr_dinfo2(signal, "thread %pt will restart syscall %lu after signal %d", current_thread, syscall_nr, next_signal);
            platform_syscall_setup_restart_context(regs, syscall_nr);
            goto real_prepare;
        }
        else
        {
            platform_syscall_store_retval(regs, -EINTR);
        }
    }
    else
    {
        platform_syscall_store_retval(regs, syscall_ret);
    }

    if (!next_signal)
        return nullptr; // no pending signal, leave asap

real_prepare:
    pr_dinfo2(signal, "thread %pt will handle signal %d with handler %p when returning from syscall", current_thread, next_signal, action.handler);
    return do_signal_exit_to_user_prepare(regs, next_signal, &action);
}

void signal_on_returned(sigreturn_data_t *data)
{
    if (!data->was_masked)
        sigset_del(&current_thread->signal_info.mask, data->signal);

    pr_dinfo2(signal, "thread %pt returned from signal %d", current_thread, data->signal);
}

bool signal_has_pending(void)
{
    bool has_pending = false;
    spinlock_acquire(&current_thread->signal_info.lock);
    for (const auto pending : current_thread->signal_info.pending)
    {
        if (sigset_test(&current_thread->signal_info.mask, pending))
            continue; // signal is masked, skip it
        has_pending = true;
        break;
    }

    spinlock_release(&current_thread->signal_info.lock);
    return has_pending;
}
