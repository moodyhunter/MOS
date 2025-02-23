// SPDX-License-Identifier: GPL-3.0-or-later
#include "global.hpp"
#include "unit/unit.hpp"

#include <argparse/libargparse.h>
#include <filesystem>
#include <functional>
#include <glob.h>
#include <iostream>
#include <map>
#include <set>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <toml++/toml.hpp>
#include <unistd.h>
#include <vector>

#define RED(text)   "\033[1;31m" text "\033[0m"
#define GREEN(text) "\033[1;32m" text "\033[0m"

#define FAILED()   RED("[FAILED]")
#define OK()       GREEN("[  OK  ]")
#define STARTING() "\033[0m         "

std::map<std::string, pid_t> service_pid;
bool debug = false;
inline GlobalConfig global_config;
inline std::map<std::string, std::shared_ptr<Unit>> units;

static std::vector<std::string> startup_order_for_unit(const std::string &id)
{
    std::set<std::string> visited;
    std::vector<std::string> order;

    std::function<void(const std::string &)> visit = [&](const std::string id)
    {
        if (visited.contains(id))
            return;

        visited.insert(id);
        const auto &unit = units[id];
        if (!unit)
        {
            std::cerr << "unit " << id << " does not exist" << std::endl;
            return;
        }

        for (const auto &dep_id : unit->depends_on)
            visit(dep_id);
        order.push_back(id);
    };

    visit(id);
    return order;
}

static bool start_unit_tree(const std::string &id)
{
    const auto order = startup_order_for_unit(id);
    for (const auto &unit_id : order)
    {
        const auto unit = units[unit_id];

        if (debug)
            std::cout << STARTING() << "Starting " << unit->description << " (" << unit->id << ")" << std::endl;
        if (!unit->start())
        {
            std::cerr << FAILED() << " Failed to start " << unit->description << ": " << unit->error_reason() << std::endl;
            return false;
        }
        else
        {
            if (unit->type == "target")
                std::cout << OK() << " Reached target " << unit->description << std::endl;
            else
                std::cout << OK() << " Started " << unit->description << std::endl;
        }
    }

    return true;
}

static void sigsegv_handler(int sig)
{
    if (sig == SIGSEGV)
    {
        std::cout << RED("INIT process received SIGSEGV") << std::endl << std::endl;
        std::cout << RED("!!!!!!!!!!!!!!!!!!!!!!!!!!") << std::endl;
        std::cout << RED("!!! Segmentation fault !!!") << std::endl;
        std::cout << RED("!!!!!!!!!!!!!!!!!!!!!!!!!!") << std::endl;
        std::cout << RED("!!!") GREEN("  Good Bye~  ") RED("!!!") << std::endl;
        while (true)
            sched_yield();
    }
}

static void sigchild_handler(int sig)
{
    MOS_UNUSED(sig);
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

    std::filesystem::path config_file = "/initrd/config/init-config.toml";
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
            case 'C': config_file = state.optarg; break;
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

    if (debug)
        std::cout << "init: using config file " << config_file << std::endl;

    if (!std::filesystem::exists(config_file))
    {
        std::cerr << "init: config file " << config_file << " does not exist" << std::endl;
        return DYN_ERROR_CODE;
    }

    load_configurations(config_file);

    if (!start_unit_tree(global_config.default_target))
    {
        std::cerr << RED("init: failed to start default target") << std::endl;
        return DYN_ERROR_CODE;
    }

    // start the shell
    const char **shell_argv = (const char **) malloc(sizeof(char *));
    int shell_argc = 1;
    shell_argv[0] = shell.c_str();

    const char *arg;
    argparse_init(&state, argv); // reset the options
    while ((arg = argparse_arg(&state)))
    {
        shell_argc++;
        shell_argv = (const char **) realloc(shell_argv, shell_argc * sizeof(char *));
        shell_argv[shell_argc - 1] = arg;
    }
    shell_argv = (const char **) realloc(shell_argv, (shell_argc + 1) * sizeof(char *));
    shell_argv[shell_argc] = NULL;

start_shell:;
    const pid_t shell_pid = fork();
    if (shell_pid == 0)
        if (execv(shell.c_str(), (char **) shell_argv) <= 0)
            return DYN_ERROR_CODE;

    while (true)
    {
        int status = 0;
        const pid_t pid = waitpid(-1, &status, 0);
        if (pid == shell_pid)
        {
            puts("init: shell exited, restarting...");
            goto start_shell;
        }

        if (pid > 0)
        {
            if (WIFEXITED(status))
                std::cout << "init: process " << pid << " exited with status " << WEXITSTATUS(status) << std::endl;
            else if (WIFSIGNALED(status))
                std::cout << "init: process " << pid << " killed by signal " << WTERMSIG(status) << std::endl;
        }

        // check if any service has exited
        for (const auto &[id, spid] : service_pid)
        {
            if (spid == -1)
                continue;

            if (pid == spid)
            {
                std::cout << "init: service " << id << " exited" << std::endl;
                service_pid[id] = -1;
            }
        }
    }

    return 0;
}
