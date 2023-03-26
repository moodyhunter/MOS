// SPDX-License-Identifier: GPL-3.0-or-later

#include "mossh.h"

#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>

void do_cd(int argc, const char *argv[])
{
    switch (argc)
    {
        case 0:
        {
            printf("cd: missing argument\n");
            break;
        }
        case 1:
        {
            if (!syscall_vfs_chdir(argv[0]))
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
    printf("MOS Shell '" PROGRAM "' Version 1\n");
    printf("Type 'help' to see this help\n");
    printf("The following commands are built-in:\n");
    printf("\n");
    for (int i = 0; builtin_commands[i].command; i++)
        printf("  %-8s  %s\n", builtin_commands[i].command, builtin_commands[i].description);
    printf("\n");
    printf("Happy hacking!\n");
}

void do_pwd(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    char buffer[4096];
    size_t size = syscall_vfs_getcwd(buffer, sizeof(buffer));
    printf("%.*s\n", (int) size, buffer);
}

void do_version(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);
    printf("MOS Shell '" PROGRAM "' Version 1\n");
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
    { .command = "cd", .action = do_cd, .description = "Change the current directory" },
    { .command = "clear", .action = do_clear, .description = "Clear the screen" },
    { .command = "exit", .action = do_exit, .description = "Exit the shell" },
    { .command = "echo", .action = do_echo, .description = "Echo arguments" },
    { .command = "help", .action = do_help, .description = "Show this help" },
    { .command = "pwd", .action = do_pwd, .description = "Print the current directory" },
    { .command = "version", .action = do_version, .description = "Show version information" },
    { .command = "which", .action = do_which, .description = "Show the full path of a command" },
    { .command = NULL, .action = NULL, .description = NULL },
};
