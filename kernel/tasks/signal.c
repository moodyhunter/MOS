// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/signal.h"

#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/tasks/signal_types.h>
#include <mos_stdlib.h>

slab_t *sigpending_slab = NULL;
SLAB_AUTOINIT("signal_pending", sigpending_slab, sigpending_t);

noreturn static void signal_do_coredump(signal_t signal)
{
    process_handle_exit(current_process, 0, signal);
}

noreturn static void signal_do_terminate(signal_t signal)
{
    process_handle_exit(current_process, 0, signal);
}

static void signal_do_ignore(signal_t signal)
{
    pr_dinfo2(signal, "thread %pt ignoring signal %d", (void *) current_thread, signal);
}

void signal_send_to_thread(thread_t *target, signal_t signal)
{
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
}

void signal_send_to_process(process_t *target, signal_t signal)
{
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
        return;
    }

    signal_send_to_thread(target_thread, signal);
}

static signal_t signal_get_next_pending(void)
{
    signal_t signal = 0;
    MOS_ASSERT(spinlock_is_locked(&current_thread->signal_info.lock));
    list_foreach(sigpending_t, sig, current_thread->signal_info.pending)
    {
        if (current_thread->signal_info.masks[sig->signal])
            continue; // signal is masked, skip it

        list_remove(sig);
        signal = sig->signal;
        kfree(sig);
        break;
    }

    return signal;
}

// clang-format off
#define _terminate() signal_do_terminate(next_signal); break
#define _coredump()  signal_do_coredump(next_signal); break
#define _ignore()    signal_do_ignore(next_signal); break
// clang-format on

void signal_check_and_handle(void)
{
    if (!current_thread)
        return;

    spinlock_acquire(&current_thread->signal_info.lock);
    const signal_t next_signal = signal_get_next_pending();
    spinlock_release(&current_thread->signal_info.lock);

    if (!next_signal)
        return; // no pending signal, leave asap

    const sigaction_t action = current_process->signal_info.handlers[next_signal];

    if (action.handler == SIG_DFL)
    {
        switch (next_signal)
        {
            case SIGINT: _terminate();
            case SIGILL: _coredump();
            case SIGTRAP: _coredump();
            case SIGABRT: _coredump();
            case SIGKILL: _terminate();
            case SIGSEGV: _coredump();
            case SIGTERM: _terminate();
            case SIGCHLD: _ignore();

            default: MOS_UNREACHABLE_X("handle this signal %d", next_signal); break;
        }

        // the default handler returns
        return;
    }

    const bool was_masked = current_thread->signal_info.masks[next_signal];
    if (!was_masked)
        current_thread->signal_info.masks[next_signal] = true;

    const sigreturn_data_t data = {
        .signal = next_signal,
        .was_masked = was_masked,
    };

    platform_jump_to_signal_handler(&data, &action); // save previous register states onto user stack
}
#undef _terminate
#undef _coredump
#undef _ignore

void signal_on_returned(sigreturn_data_t *data)
{
    if (!data->was_masked)
        current_thread->signal_info.masks[data->signal] = false;
}
