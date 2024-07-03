// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <argparse/libargparse.h>
#include <ctime>
#include <iostream>
#include <libconfig/libconfig.h>
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

static const config_t *config;

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
    size_t num_drivers;
    const char **services = config_get_all(config, "service", &num_drivers);
    if (!services)
        return true;

    for (size_t i = 0; i < num_drivers; i++)
    {
        const auto service = services[i];
        const auto driver_pid = fork();
        if (driver_pid == 0)
        {
            // child
            execl(service, service, NULL);
            exit(-1);
        }

        if (driver_pid <= 0)
            fprintf(stderr, "Failed to start service: %s\n", service);
    }

    free(services);
    return true;
}

static bool create_directories(void)
{
    size_t num_dirs;
    const char **dirs = config_get_all(config, "mkdir", &num_dirs);
    if (!dirs)
        return false;

    for (size_t i = 0; i < num_dirs; i++)
    {
        const char *dir = dirs[i];
        if (mkdir(dir, 0755) != 0)
            return false;
    }

    return true;
}

static bool create_symlinks(void)
{
    size_t num_symlinks;
    const char **symlinks = config_get_all(config, "symlink", &num_symlinks);
    if (!symlinks)
        return false;

    for (size_t i = 0; i < num_symlinks; i++)
    {
        // format: <Source> <Destination>
        const std::string line = symlinks[i];

        // split the string
        const auto source = string_trim(line.substr(0, line.find(' ')));
        const auto destination = string_trim(line.substr(line.find(' ') + 1));

        if (source.empty() || destination.empty())
            return false;

        if (link(source.c_str(), destination.c_str()) != 0)
            return false;
    }

    return true;
}

static bool mount_filesystems(void)
{
    size_t num_mounts;
    const char **mounts = config_get_all(config, "mount", &num_mounts);
    if (!mounts)
        return false;

    for (size_t i = 0; i < num_mounts; i++)
    {
        // format: <Device> <MountPoint> <Filesystem> <Options>
        const std::string mount = mounts[i];

        // split the string
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

        if (syscall_vfs_mount(device.c_str(), mount_point.c_str(), filesystem.c_str(), options.c_str()) != 0)
            return false;
    }

    return true;
}

static void sigsegv_handler(int sig)
{
    if (sig == SIGSEGV)
    {
        puts("\033[1;31mSegmentation fault\033[0m");
        while (true)
            sched_yield();
    }
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
    sigaction(SIGCHLD, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

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
        puts("init: not running as PID 1");

        for (int i = 0; i < argc; i++)
            printf("argv[%d] = %s\n", i, argv[i]);

        puts("Leaving init...");
        return DYN_ERROR_CODE;
    }

    config = config_parse_file(config_file);
    if (!config)
        return DYN_ERROR_CODE;

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
        pid_t pid = waitpid(-1, NULL, 0);
        if (pid == shell_pid)
        {
            puts("init: shell exited, restarting...");
            goto start_shell;
        }
        else
        {
            printf("init: process %d exited\n", pid);
        }
    }

    return 0;
}
