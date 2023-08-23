// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <mos/tasks/signal_types.h>
#include <signal.h>
#include <stdlib.h>

noreturn static __attribute__((naked)) void sigreturn_trampoline(void)
{
#if defined(__x86_64__)
    __asm__ volatile("movq %%rsp, %%rdi\ncall *%0\n" ::"a"(syscall_signal_return));
#else
#error "Unsupported architecture"
#endif
}

int raise(signal_t sig)
{
    syscall_signal_thread(syscall_get_tid(), sig);
    return 0;
}

int kill(pid_t pid, signal_t sig)
{
    return 0;
}

int sigaction(signal_t sig, sighandler *handler)
{
    sigaction_t *act = malloc(sizeof(sigaction_t));
    act->handler = *handler;
    act->sigreturn_trampoline = (void *) (ptr_t) sigreturn_trampoline;
    syscall_signal_register(sig, act);
    return 0;
}
