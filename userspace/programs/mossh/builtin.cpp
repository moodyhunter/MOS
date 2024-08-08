// SPDX-License-Identifier: GPL-3.0-or-later

#include "mossh.hpp"

#include <errno.h>
#include <iostream>
#include <map>
#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unistd.h>

std::map<std::string, std::string> aliases = {};

static void greet(void)
{
    puts("MOS Shell Version 1");
}

void do_alias(const std::vector<std::string> &argv)
{
    if (argv.empty())
    {
        for (const auto &[name, value] : aliases)
            printf("alias: '%s' -> '%s'\n", name.c_str(), value.c_str());
        return;
    }

    if (argv.size() != 2)
    {
        printf("alias: wrong number of arguments\n");
        printf("Usage: alias <name> <value>\n");
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
        printf("replace alias '%s': '%s' -> '%s'\n", name.c_str(), aliases[name].c_str(), value.c_str());
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
                printf("cd: /: Unexpected error: %s\n", strerror(errno));
            break;
        }
        case 1:
        {
            if (chdir(argv[0].c_str()))
                printf("cannot change directory to '%s': %s\n", argv[0].c_str(), strerror(errno));
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

void do_echo(const std::vector<std::string> &argv)
{
    for (size_t i = 0; i < argv.size(); i++)
        printf("%s ", argv[i].c_str());
    printf("\n");
}

void do_export(const std::vector<std::string> &argv)
{
    if (argv.size() == 0)
    {
        printf("export: wrong number of arguments\n");
        printf("Usage: export <name=value> ...\n");
        return;
    }

    for (const auto &arg : argv)
    {
        const auto pos = arg.find('=');
        if (pos == std::string::npos)
        {
            printf("export: invalid argument: '%s'\n", arg.c_str());
            continue;
        }

        const auto name = arg.substr(0, pos);
        auto value = arg.substr(pos + 1);

        if ((value.starts_with('\'') && value.ends_with('\'')) || (value.starts_with('"') && value.ends_with('"')))
            value = value.substr(1, value.size() - 2);

        if (verbose)
            printf("export: '%s' -> '%s'\n", name.c_str(), value.c_str());

        setenv(name.c_str(), value.c_str(), 1);

        if (name == "PATH")
            get_paths(true); // clear the cache
    }
}

void do_exit(const std::vector<std::string> &argv)
{
    if (argv.size() != 0)
        exit(0);

    const int exit_code = atoi(argv[0].c_str());
    exit(exit_code);
}

void do_help(const std::vector<std::string> &argv)
{
    MOS_UNUSED(argv);
    greet();
    printf("Type 'help' to see this help\n");
    printf("The following commands are built-in:\n");
    printf("\n");
    for (int i = 0; builtin_commands[i].command; i++)
        printf("  %-10s  %s\n", builtin_commands[i].command, builtin_commands[i].description);
    printf("\n");
    printf("Happy hacking!\n");
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
    printf("pid: %d\n", getpid());
}

void do_pwd(const std::vector<std::string> &argv)
{
    MOS_UNUSED(argv);
    char buffer[4096];
    void *p = getcwd(buffer, sizeof(buffer));
    printf("%s\n", (char *) p);
}

void do_repeat(const std::vector<std::string> &argv)
{
    switch (argv.size())
    {
        case 0:
        case 1:
        {
            printf("usage: repeat <count> <command> [args...]\n");
            break;
        }
        default:
        {
            int count = atoi(argv[0].c_str());
            if (count <= 0)
            {
                printf("repeat: invalid count: '%s'\n", argv[0].c_str());
                break;
            }

            for (int i = 0; i < count; i++)
            {
                const auto program = argv[1];
                const auto new_argv = std::vector<std::string>(argv.begin() + 1, argv.end());

                if (!do_execute(program, new_argv, true))
                {
                    std::cerr << "repeat: failed to execute '" << program << "'" << std::endl;
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
            printf("which: missing argument\n");
            break;
        }
        case 1:
        {
            const auto location = locate_program(argv[0]);
            if (location)
                printf("%s\n", location->c_str());
            else
                printf("which: %s: command not found\n", argv[0].c_str());
            break;
        }
        default:
        {
            printf("which: too many arguments\n");
            break;
        }
    }
}

const command_t builtin_commands[] = {
    { .command = "alias", .action = do_alias, .description = "Create an alias" },
    { .command = "cd", .action = do_cd, .description = "Change the current directory" },
    { .command = "clear", .action = do_clear, .description = "Clear the screen" },
    { .command = "exit", .action = do_exit, .description = "Exit the shell" },
    { .command = "echo", .action = do_echo, .description = "Echo arguments" },
    { .command = "export", .action = do_export, .description = "Export a variable" },
    { .command = "help", .action = do_help, .description = "Show this help" },
    { .command = "msleep", .action = do_msleep, .description = "Sleep for a number of milliseconds" },
    { .command = "pid", .action = do_pid, .description = "Show the current process ID" },
    { .command = "pwd", .action = do_pwd, .description = "Print the current directory" },
    { .command = "repeat", .action = do_repeat, .description = "Repeat a command a number of times" },
    { .command = "show-path", .action = do_show_path, .description = "Show the search path for programs" },
    { .command = "sleep", .action = do_sleep, .description = "Sleep for a number of seconds" },
    { .command = "source", .action = do_source, .description = "Execute a script" },
    { .command = "version", .action = do_version, .description = "Show version information" },
    { .command = "which", .action = do_which, .description = "Show the full path of a command" },
    { .command = NULL, .action = NULL, .description = NULL },
};
