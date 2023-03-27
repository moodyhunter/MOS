// SPDX-License-Identifier: GPL-3.0-or-later

#include "argparse/libargparse.h"
#include "mossh.h"
#include "readline/libreadline.h"

#include <mos/filesystem/fs_types.h>
#include <mos/kconfig.h>
#include <mos/mos_global.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// We don't support environment variables yet, so we hardcode the path
const char *PATH[] = {
    "/programs",
    "/initrd/programs",
    NULL,
};

bool do_program(const char *prog, int argc, const char **argv)
{
    prog = locate_program(prog);
    if (!prog)
        return false;

    pid_t pid = syscall_spawn(prog, argc, argv);
    if (pid == -1)
    {
        fprintf(stderr, "Failed to execute '%s'\n", prog);
        return false;
    }

    syscall_wait_for_process(pid);
    free((void *) prog);
    return true;
}

bool do_builtin(const char *command, int argc, const char **argv)
{
    for (int i = 0; builtin_commands[i].command; i++)
    {
        if (strcmp(command, builtin_commands[i].command) == 0)
        {
            builtin_commands[i].action(argc, argv);
            return true;
        }
    }

    return false;
}

static void do_execute_line(char *line)
{
    const char *token = strtok(line, " ");
    if (!token)
        return;

    const char *prog = token;
    token = strtok(NULL, " ");

    int new_argc = 0;
    const char **new_argv = malloc(sizeof(char *));
    while (token)
    {
        new_argv = realloc(new_argv, sizeof(char *) * (new_argc + 1));
        new_argv[new_argc] = token;
        new_argc++;
        token = strtok(NULL, " ");
    }

    if (!do_builtin(prog, new_argc, new_argv))
        if (!do_program(prog, new_argc, new_argv))
            fprintf(stderr, "'%s' is not recognized as an internal, operable program or batch file.\n", prog);

    free(new_argv);
}

static int do_interpret_script(const char *path)
{
    fd_t fd = syscall_vfs_open(path, OPEN_READ);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to open '%s'\n", path);
        return 1;
    }

    char *line = get_line(fd);
    while (line)
    {
        printf("<script>: %s\n", line);
        do_execute_line(line);
        free(line);
        line = get_line(fd);
    }

    syscall_io_close(fd);
    return 0;
}

static const argparse_arg_t mossh_options[] = {
    { "help", 'h', ARGPARSE_NONE },
    { "version", 'v', ARGPARSE_NONE },
    { NULL, 'c', ARGPARSE_REQUIRED }, // command to execute
    { 0 },
};

int main(int argc, const char **argv)
{
    MOS_UNUSED(argc);

    argparse_state_t state;
    argparse_init(&state, argv);

    while (true)
    {
        const int option = argparse_long(&state, mossh_options, NULL);
        if (option == -1)
            break;

        switch (option)
        {
            case 'h': do_execute_line("help"); return 0;
            case 'v': do_execute_line("version"); return 0;
            case 'c': return do_interpret_script(argv[2]);
            default: do_execute_line("help"); return 1;
        }
    }

    printf("Welcome to Mosh!\n");
    char cwdbuf[MOS_PATH_MAX_LENGTH] = { 0 };

    while (1)
    {
        const ssize_t sz = syscall_vfs_getcwd(cwdbuf, MOS_PATH_MAX_LENGTH);

        if (sz <= 0)
        {
            fputs("Failed to get current working directory.\n", stderr);
            cwdbuf[0] = '?';
            cwdbuf[1] = '\0';
        }

        const char *prompt_part2 = " > ";
        const size_t prompt_len = strlen(cwdbuf) + strlen(prompt_part2) + 1;
        char *prompt = malloc(prompt_len);
        memzero(prompt, prompt_len);
        sprintf((char *) prompt, "%s%s", cwdbuf, prompt_part2);

        char *line = readline(prompt);
        do_execute_line(line);
        free(line);
        free((void *) prompt);
    }

    return 0;
}
