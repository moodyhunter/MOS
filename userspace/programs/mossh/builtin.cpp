// SPDX-License-Identifier: GPL-3.0-or-later

#include "LaunchContext.hpp"
#include "mos/syscall/usermode.h"
#include "mossh.hpp"

#include <errno.h>
#include <iostream>
#include <map>
#include <mos/types.h>
#include <ranges>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

std::map<std::string, std::string> aliases = {};
static void greet(void)
{
    puts("MOS Shell Version 2");
}

void do_alias(const std::vector<std::string> &argv)
{
    if (argv.empty())
    {
        for (const auto &[name, value] : aliases)
            std::cerr << "alias: '" << name << "' -> '" << value << "'" << std::endl;
        return;
    }

    if (argv.size() != 2)
    {
        std::cerr << "alias: wrong number of arguments" << std::endl;
        std::cerr << "Usage: alias <name> <value>" << std::endl;
        return;
    }

    const auto &[name, value] = std::tie(argv[0], argv[1]);
    if (name == "-c")
    {
        // remove 'value' alias if it exists
        if (aliases.contains(value))
        {
            aliases.erase(value);
            return;
        }
    }

    if (aliases.contains(name))
    {
        if (aliases[name] == value)
            return; // no change

        std::cout << "alias: replace alias '" << name << "': '" << aliases[name] << "' -> '" << value << "'" << std::endl;
        aliases[name] = value;
        return;
    }

    if (verbose)
        std::cout << "alias: '" << name << "' -> '" << value << "'\n";
    aliases[name] = value;
}

void do_cd(const std::vector<std::string> &argv)
{
    switch (argv.size())
    {
        case 0:
        {
            if (chdir("/"))
                std::cerr << "cd: /: Unexpected error: " << strerror(errno) << std::endl;
            break;
        }
        case 1:
        {
            if (chdir(argv[0].c_str()))
                std::cerr << "cd: " << argv[0] << ": " << strerror(errno) << std::endl;
            break;
        }
        default:
        {
            printf("cd: too many arguments\n");
            break;
        }
    }
}

void do_clear(const std::vector<std::string> &argv)
{
    MOS_UNUSED(argv);
    printf("\033[2J\033[H");
}

void do_export(const std::vector<std::string> &argv)
{
    if (argv.size() == 0)
    {
        puts("export: wrong number of arguments\n");
        puts("Usage: export <name=value> ...\n");
        return;
    }

    for (const auto &arg : argv)
    {
        const auto pos = arg.find('=');
        if (pos == std::string::npos)
        {
            std::cout << "export: invalid argument: '" << arg << "'" << std::endl;
            continue;
        }

        const auto name = arg.substr(0, pos);
        auto value = arg.substr(pos + 1);

        if ((value.starts_with('\'') && value.ends_with('\'')) || (value.starts_with('"') && value.ends_with('"')))
            value = value.substr(1, value.size() - 2);

        if (verbose)
            std::cout << "export: '" << name << "' -> '" << value << "'" << std::endl;

        setenv(name.c_str(), value.c_str(), 1);

        if (name == "PATH")
            get_paths(true); // clear the cache
    }
}

void do_exit(const std::vector<std::string> &argv)
{
    if (argv.size() == 0)
        exit(0);

    const int exit_code = atoi(argv[0].c_str());
    exit(exit_code);
}

void do_help(const std::vector<std::string> &argv)
{
    MOS_UNUSED(argv);
    greet();
    puts("Type 'help' to see this help\n");
    puts("The following commands are built-in:\n");
    for (const auto &command : builtin_commands)
        printf("  %-10s  %s\n", command.command, command.description);
    puts("Happy hacking!\n");
}

void do_msleep(const std::vector<std::string> &argv)
{
    if (argv.size() != 1)
    {
        printf("msleep: wrong number of arguments\n");
        printf("Usage: msleep <ms>\n");
        return;
    }

    u64 ms = atoi(argv[0].c_str());
    if (ms <= 0)
    {
        printf("msleep: invalid argument: '%s'\n", argv[0].c_str());
        return;
    }

    usleep(ms * 1000); // usleep takes microseconds, we take milliseconds
}

void do_pid(const std::vector<std::string> &argv)
{
    MOS_UNUSED(argv);
    std::cout << "pid: " << getpid() << std::endl;
}

void do_rand(const std::vector<std::string> &argv)
{
    MOS_UNUSED(argv);
    std::cout << rand() << std::endl;
}

void do_repeat(const std::vector<std::string> &argv)
{
    switch (argv.size())
    {
        case 0:
        case 1:
        {
            puts("usage: repeat <count> <command> [args...]\n");
            break;
        }
        default:
        {
            int count = atoi(argv[0].c_str());
            if (count <= 0)
            {
                std::cerr << "repeat: invalid count: '" << argv[0] << "'" << std::endl;
                break;
            }

            const auto program = argv[1];
            const auto new_argv = argv | std::views::drop(1) | std::ranges::to<std::vector>();

            for (int i = 0; i < count; i++)
            {
                LaunchContext context{ new_argv };
                if (!context.start())
                {
                    std::cerr << "repeat: failed to start '" << program << "'" << std::endl;
                    break;
                }
            }
            break;
        }
    }
}

void do_show_path(const std::vector<std::string> &argv)
{
    MOS_UNUSED(argv);

    puts("Program search path:");
    for (const auto &path : get_paths())
        printf("  %s\n", path.c_str());
}

void do_sleep(const std::vector<std::string> &argv)
{
    if (argv.size() != 1)
    {
        printf("sleep: wrong number of arguments\n");
        printf("Usage: sleep <seconds>\n");
        return;
    }

    int seconds = atoi(argv[0].c_str());
    if (seconds <= 0)
    {
        printf("sleep: invalid argument: '%s'\n", argv[0].c_str());
        return;
    }

    sleep(seconds);
}

void do_source(const std::vector<std::string> &argv)
{
    if (argv.size() != 1)
    {
        printf("source: wrong number of arguments\n");
        printf("Usage: source <file>\n");
        return;
    }

    do_interpret_script(argv[0]);
}

void do_version(const std::vector<std::string> &argv)
{
    MOS_UNUSED(argv);
    greet();
}

void do_which(const std::vector<std::string> &argv)
{
    switch (argv.size())
    {
        case 0:
        {
            puts("which: missing argument\n");
            break;
        }
        case 1:
        {
            LaunchContext context({ argv[0] });
            if (!context.resolve_program_path())
            {
                printf("which: %s: command not found\n", argv[0].c_str());
                break;
            }

            printf("%s\n", context.program_path().c_str());
            break;
        }
        default:
        {
            printf("which: too many arguments\n");
            break;
        }
    }
}

void do_kmodload(const std::vector<std::string> &)
{
    syscall_kmod_load("/initrd/kmods/kmodtest.o");
}

const std::vector<command_t> builtin_commands = {
    { .command = "alias", .action = do_alias, .description = "Create an alias" },
    { .command = "cd", .action = do_cd, .description = "Change the current directory" },
    { .command = "clear", .action = do_clear, .description = "Clear the screen" },
    { .command = "exit", .action = do_exit, .description = "Exit the shell" },
    { .command = "export", .action = do_export, .description = "Export a variable" },
    { .command = "help", .action = do_help, .description = "Show this help" },
    { .command = "msleep", .action = do_msleep, .description = "Sleep for a number of milliseconds" },
    { .command = "pid", .action = do_pid, .description = "Show the current process ID" },
    { .command = "rand", .action = do_rand, .description = "Generate a random number" },
    { .command = "repeat", .action = do_repeat, .description = "Repeat a command a number of times" },
    { .command = "show-path", .action = do_show_path, .description = "Show the search path for programs" },
    { .command = "sleep", .action = do_sleep, .description = "Sleep for a number of seconds" },
    { .command = "source", .action = do_source, .description = "Execute a script" },
    { .command = "version", .action = do_version, .description = "Show version information" },
    { .command = "kmod", .action = do_kmodload, .description = "Load a kernel module" },
    { .command = "which", .action = do_which, .description = "Show the full path of a command" },
};
