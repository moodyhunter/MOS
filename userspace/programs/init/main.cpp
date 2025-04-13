// SPDX-License-Identifier: GPL-3.0-or-later
#include "ServiceManager.hpp"
#include "global.hpp"
#include "logging.hpp"
#include "rpc/UnitStateReceiver.hpp"
#include "rpc/rpc.hpp"

#include <argparse/libargparse.h>
#include <iostream>
#include <stacktrace>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

static void sigsegv_handler(int sig)
{
    std::cout << std::stacktrace::current() << std::endl << std::flush;
    if (sig == SIGSEGV)
    {
        std::cout << RED("INIT process received SIGSEGV") << std::endl << std::endl;
        std::cout << RED("!!!!!!!!!!!!!!!!!!!!!!!!!!") << std::endl;
        std::cout << RED("!!! Segmentation fault !!!") << std::endl;
        std::cout << RED("!!!!!!!!!!!!!!!!!!!!!!!!!!") << std::endl;
        std::cout << RED("!!!") GREEN("  Good Bye~  ") RED("!!!") << std::endl;
        std::cout << RED("!!!!!!!!!!!!!!!!!!!!!!!!!!") << std::endl;
        while (true)
            sched_yield();
    }
}

static void sigchild_handler(int sig)
{
    (void) sig;
}

#define DYN_ERROR_CODE (__COUNTER__ + 1)

static const argparse_arg_t longopts[] = {
    { "help", 'h', ARGPARSE_NONE, "show this help" },
    { "config", 'C', ARGPARSE_REQUIRED, "configuration file, default: /initrd/config/init.conf" },
    { "shell", 'S', ARGPARSE_REQUIRED, "shell to start, default: /initrd/programs/mossh" },
    {},
};

int main(int argc, const char *argv[])
{
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sigchild_handler;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigsegv_handler;
    sigaction(SIGSEGV, &sa, NULL);

    std::filesystem::path configPath = "/initrd/config/init-config.toml";
    std::string shell = "/initrd/programs/mossh";

    argparse_state_t state;
    argparse_init(&state, argv);
    while (true)
    {
        const int option = argparse_long(&state, longopts, NULL);
        if (option == -1)
            break;

        switch (option)
        {
            case 'C': configPath = state.optarg; break;
            case 'S': shell = state.optarg; break;
            case 'h': argparse_usage(&state, longopts, "the init program"); return 0;
            default: break;
        }
    }

    if (getpid() != 1)
    {
        for (int i = 0; i < argc; i++)
            printf("argv[%d] = %s\n", i, argv[i]);
        puts("init: not running as PID 1, exiting...");
        return DYN_ERROR_CODE;
    }

    Debug << "init: using config file " << configPath << std::endl;

    if (!std::filesystem::exists(configPath))
    {
        std::cerr << "init: config file " << configPath << " does not exist" << std::endl;
        return DYN_ERROR_CODE;
    }

    ServiceManager->LoadConfiguration(ReadAllConfig(configPath));
    std::thread([]() { RpcServer->run(); }).detach();
    std::thread([]() { UnitStateReceiverService->run(); }).detach();

    if (!ServiceManager->StartDefaultTarget())
    {
        std::cerr << RED("init: failed to start default target") << std::endl;
        return DYN_ERROR_CODE;
    }

    // start the shell
    argparse_init(&state, argv); // reset the options

    std::vector<const char *> args;
    args.push_back(shell.c_str());
    while (const auto arg = argparse_arg(&state))
        args.push_back(arg);
    args.push_back(nullptr);

    const static auto StartShell = [&]()
    {
        const pid_t pid = fork();
        if (pid == 0)
        {
            if (execv(shell.c_str(), (char **) args.data()) <= 0)
            {
                std::cout << "Failed to start shell" << std::endl;
                exit(1);
            }
        }
        return pid;
    };

    pid_t shell_pid = StartShell();

    while (true)
    {
        int status = 0;
        const pid_t pid = waitpid(-1, &status, 0);
        if (pid == shell_pid)
        {
            puts("init: shell exited, restarting...");
            shell_pid = StartShell();
        }

        ServiceManager->OnProcessExit(pid, status);
    }

    return 0;
}
