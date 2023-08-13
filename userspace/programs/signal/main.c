// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <mos/tasks/signal_types.h>
#include <stdio.h>

void sigint_handler(signal_t signum)
{
    printf("SIGINT(%d) received\n", (u32) signum);
}

__attribute__((naked)) noreturn void sigreturn_trampoline(void)
{
    __asm__ volatile("movq %%rsp, %%rdi\n"
                     "call %0\n" ::"a"(syscall_signal_return));
}

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    sigaction_t action = {
        .handler = sigint_handler,
        .sigreturn_trampoline = (void *) (ptr_t) sigreturn_trampoline,
    };
    syscall_signal_register(SIGINT, &action);
    syscall_signal_thread(syscall_get_tid(), SIGINT);

    puts("Hello, world!");

    return 0;
}
