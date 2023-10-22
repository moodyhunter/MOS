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
    MOS_UNUSED(signal);
    pr_warn("coredump: WIP");
    process_handle_exit(current_process, 1);
}

noreturn static void signal_do_terminate(signal_t signal)
{
    MOS_UNUSED(signal);
    pr_warn("terminate: WIP");
    process_handle_exit(current_process, 1);
}

static void signal_do_ignore(signal_t signal)
{
    mos_debug(signal, "thread %pt ignoring signal %d", (void *) current_thread, signal);
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

void signal_check_and_handle(void)
{
    if (!current_thread)
        return;

    spinlock_acquire(&current_thread->signal_info.lock);
    const signal_t next_signal = signal_get_next_pending();
    spinlock_release(&current_thread->signal_info.lock);

    if (!next_signal)
        return; // no pending signal, leave asap

    sigaction_t action = { 0 };

    switch (next_signal)
    {
#define SELECT_SIGNAL_HANDLER_OR(SIGNAL, OR)                                                                                                                             \
    case SIGNAL:;                                                                                                                                                        \
        action = current_process->signal_handlers[SIGNAL];                                                                                                               \
        if (!action.handler)                                                                                                                                             \
            signal_do_##OR(SIGNAL);                                                                                                                                      \
        break

        SELECT_SIGNAL_HANDLER_OR(SIGINT, terminate);
        SELECT_SIGNAL_HANDLER_OR(SIGILL, coredump);
        SELECT_SIGNAL_HANDLER_OR(SIGTRAP, coredump);
        SELECT_SIGNAL_HANDLER_OR(SIGABRT, coredump);
        SELECT_SIGNAL_HANDLER_OR(SIGKILL, terminate);
        SELECT_SIGNAL_HANDLER_OR(SIGSEGV, coredump);
        SELECT_SIGNAL_HANDLER_OR(SIGTERM, terminate);
        SELECT_SIGNAL_HANDLER_OR(SIGCHLD, ignore);

        default: MOS_UNREACHABLE_X("handle this signal %d", next_signal); break;
    }

    if (!action.handler)
        return; // the default handler is to 'ignore', so we just leave

    const bool was_masked = current_thread->signal_info.masks[next_signal];
    if (!was_masked)
        current_thread->signal_info.masks[next_signal] = true;

    const sigreturn_data_t data = {
        .signal = next_signal,
        .was_masked = was_masked,
    };

    platform_jump_to_signal_handler(&data, &action); // save previous register states onto user stack
}

void signal_on_returned(sigreturn_data_t *data)
{
    if (!data->was_masked)
        current_thread->signal_info.masks[data->signal] = false;
}
