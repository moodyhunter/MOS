// SPDX-License-Identifier: GPL-3.0-or-later

#include "mossh.h"

#include <mos/mos_global.h>
#include <mos/syscall/usermode.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

alias_t *alias_list;
size_t alias_count;

static void greet(void)
{
    puts("MOS Shell Version 1");
}

void do_alias(int argc, const char *argv[])
{
    if (argc == 0)
    {
        for (size_t i = 0; i < alias_count; i++)
            printf("alias: '%s' -> '%s'\n", alias_list[i].name, alias_list[i].command);
        return;
    }

    if (argc != 2)
    {
        printf("alias: wrong number of arguments\n");
        printf("Usage: alias <name> <value>\n");
        return;
    }

    const char *name = argv[0];
    const char *value = argv[1];

    if (strcmp(name, "-c") == 0)
    {
        // remove 'value' alias if it exists
        for (size_t i = 0; i < alias_count; i++)
        {
            if (strcmp(value, alias_list[i].name) == 0)
            {
                free(alias_list[i].name);
                free(alias_list[i].command);
                alias_count--;
                for (size_t j = i; j < alias_count; j++)
                    alias_list[j] = alias_list[j + 1];
                return;
            }
        }
        printf("alias: '%s' not found\n", value);
        return;
    }

    for (size_t i = 0; i < alias_count; i++)
    {
        if (strcmp(name, alias_list[i].name) == 0)
        {
            if (strcmp(value, alias_list[i].command) == 0)
                return; // no change
            printf("replace alias '%s': '%s' -> '%s'\n", name, alias_list[i].command, value);
            free(alias_list[i].command);
            alias_list[i].command = strdup(value);
            return;
        }
    }

    printf("alias: '%s' -> '%s'\n", argv[0], argv[1]);
    alias_list = realloc(alias_list, sizeof(alias_t) * (alias_count + 1));
    alias_list[alias_count].name = strdup(argv[0]);
    alias_list[alias_count].command = strdup(argv[1]);
    alias_count++;
}

void do_cd(int argc, const char *argv[])
{
    switch (argc)
    {
        case 0:
        {
            if (syscall_vfs_chdir("/") != 0)
                printf("cd: /: Unexpected error\n");
            break;
        }
        case 1:
        {
            if (syscall_vfs_chdir(argv[0]) != 0)
                printf("cd: %s: No such file or directory\n", argv[0]);
            break;
        }
        default:
        {
            printf("cd: too many arguments\n");
            break;
        }
    }
}

void do_clear(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    printf("\033[2J\033[H");
}

void do_echo(int argc, const char *argv[])
{
    for (int i = 0; i < argc; i++)
        printf("%s ", argv[i]);
    printf("\n");
}

void do_exit(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    printf("Bye!\n");
    exit(0);
}

void do_help(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
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

void do_msleep(int argc, const char *argv[])
{
    if (argc != 1)
    {
        printf("msleep: wrong number of arguments\n");
        printf("Usage: msleep <ms>\n");
        return;
    }

    int ms = atoi(argv[0]);
    if (ms <= 0)
    {
        printf("msleep: invalid argument: '%s'\n", argv[0]);
        return;
    }

    syscall_clock_msleep(ms);
}

void do_pid(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    printf("pid: %d\n", syscall_get_pid());
}

void do_pwd(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    char buffer[4096];
    size_t size = syscall_vfs_getcwd(buffer, sizeof(buffer));
    printf("%.*s\n", (int) size, buffer);
}

void do_repeat(int argc, const char *argv[])
{
    switch (argc)
    {
        case 0:
        case 1:
        {
            printf("usage: repeat <count> <command> [args...]\n");
            break;
        }
        default:
        {
            int count = atoi(argv[0]);
            if (count <= 0)
            {
                printf("repeat: invalid count: '%s'\n", argv[0]);
                break;
            }

            for (int i = 0; i < count; i++)
            {
                const char *program = argv[1];
                int new_argc = argc - 1;
                const char **new_argv = &argv[1];

                if (do_builtin(program, new_argc, new_argv))
                    continue;

                if (!do_program(program, new_argc, new_argv))
                    printf("repeat: %s: command not found\n", program);
            }
            break;
        }
    }
}

void do_show_path(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    puts("Program search path:");
    for (int i = 0; PATH[i]; i++)
        printf("%2d: %s\n", i, PATH[i]);
}

void do_sleep(int argc, const char *argv[])
{
    if (argc != 1)
    {
        printf("sleep: wrong number of arguments\n");
        printf("Usage: sleep <seconds>\n");
        return;
    }

    int seconds = atoi(argv[0]);
    if (seconds <= 0)
    {
        printf("sleep: invalid argument: '%s'\n", argv[0]);
        return;
    }

    syscall_clock_msleep(seconds * 1000);
}

void do_source(int argc, const char *argv[])
{
    if (argc != 1)
    {
        printf("source: wrong number of arguments\n");
        printf("Usage: source <file>\n");
        return;
    }

    do_interpret_script(argv[0]);
}

void do_version(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    greet();
}

void do_which(int argc, const char *argv[])
{
    switch (argc)
    {
        case 0:
        {
            printf("which: missing argument\n");
            break;
        }
        case 1:
        {
            const char *location = locate_program(argv[0]);
            if (location)
            {
                printf("%s\n", location);
                free((void *) location);
            }
            else
            {
                printf("which: %s: command not found\n", argv[0]);
            }
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
