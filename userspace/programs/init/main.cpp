// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <argparse/libargparse.h>
#include <ctime>
#include <iostream>
#include <libconfig/libconfig.h>
#include <map>
#include <mos/syscall/usermode.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

static const bool debug = false;

static Config config;
static std::map<std::string, pid_t> service_pid;

static std::string string_trim(const std::string &in)
{
    std::string str = in;

    // Trim leading space
    while (str[0] == ' ')
        str.erase(0, 1);

    if (str.empty()) // All spaces?
        return str;

    // Trim trailing space
    while (str.back() == ' ')
        str.pop_back();

    return str;
}

bool start_services(void)
{
    const auto services = config.get_entries("services");

    for (const auto &[service_name, executable] : services)
    {
        if (debug)
            std::cout << "Starting service: " << service_name << " -> " << executable << std::endl;

        const auto driver_pid = fork();

        if (driver_pid == 0) // child
        {
            execl(executable.c_str(), executable.c_str(), NULL);
            exit(-1);
        }
        else if (driver_pid < 0)
        {
            std::cerr << "Failed to start service: " << executable << std::endl;
        }
        else // parent
        {
            std::string name = service_name;
            if (service_pid.contains(service_name))
                std::cerr << "Duplicated service name: " << service_name << ", renaming as " << service_name << ".1" << std::endl, name += ".1";
            service_pid[name] = driver_pid;
        }
    }

    return true;
}

static bool create_directories(void)
{
    const auto mkdirs = config.get_entries("mkdir");

    for (const auto &[_, dir] : mkdirs)
    {
        if (debug)
            std::cout << "Creating directory: " << dir << std::endl;

        if (mkdir(dir.c_str(), 0755) != 0)
            return false;
    }

    return true;
}

static bool create_symlinks(void)
{
    const auto symlinks = config.get_entries("symlink");

    for (const auto &[_, value] : symlinks)
    {
        // decompose the value (source destination)
        const auto pos = value.find(' ');
        if (pos == std::string::npos)
            return false;

        const auto source = string_trim(value.substr(0, pos));
        const auto destination = string_trim(value.substr(pos + 1));

        if (debug)
            std::cout << "Creating symlink: " << source << " -> " << destination << std::endl;

        if (link(source.c_str(), destination.c_str()) != 0)
            return false;
    }

    return true;
}

static bool mount_filesystems(void)
{
    const auto mounts = config.get_entries("mount");

    for (const auto &[_, mount] : mounts)
    {
        // format: <Device> <MountPoint> <Filesystem> <Options>
        std::vector<std::string> parts = { "", "", "", "" };
        size_t part = 0;
        for (const char c : mount)
        {
            if (c == ' ')
                part++;
            else
                parts[part] += c;
        }

        if (parts.size() != 4)
            std::cerr << "Invalid mount line: " << mount << std::endl;

        std::ranges::for_each(parts, [](auto &part) { part = string_trim(part); });

        if (std::ranges::any_of(parts, &std::string::empty))
            return false;

        const auto [device, mount_point, filesystem, options] = std::make_tuple(parts[0], parts[1], parts[2], parts[3]);

        if (debug)
            std::cout << "Mounting: " << device << " -> " << mount_point << " (" << filesystem << ") with options: " << options << std::endl;

        if (syscall_vfs_mount(device.c_str(), mount_point.c_str(), filesystem.c_str(), options.c_str()) != 0)
            return false;
    }

    return true;
}

static void sigsegv_handler(int sig)
{
    if (sig == SIGSEGV)
    {
#define RED   "\033[1;31m"
#define RESET "\033[0m"

        std::cout << RED << "Segmentation fault" << RESET << std::endl;
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

    const char *config_file = "/initrd/config/init.conf";
    const char *shell = "/initrd/programs/mossh";
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

    if (const auto result = Config::from_file(config_file); result)
        config = result.value();
    else
    {
        std::cerr << "Failed to parse config file: " << config_file << std::endl;
        return DYN_ERROR_CODE;
    }

    if (!create_directories())
        return DYN_ERROR_CODE;

    if (!create_symlinks())
        return DYN_ERROR_CODE;

    if (!mount_filesystems())
        return DYN_ERROR_CODE;

    if (!start_services())
        return DYN_ERROR_CODE;

    // start the shell
    const char **shell_argv = (const char **) malloc(sizeof(char *));
    int shell_argc = 1;
    shell_argv[0] = shell;

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
        if (execv(shell, (char **) shell_argv) <= 0)
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
    }

    return 0;
}
