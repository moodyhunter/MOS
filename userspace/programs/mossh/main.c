// SPDX-License-Identifier: GPL-3.0-or-later

#include "argparse/libargparse.h"
#include "mossh.h"
#include "readline/libreadline.h"

#include <fcntl.h>
#include <mos/filesystem/fs_types.h>
#include <mos/kconfig.h>
#include <mos/mos_global.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// We don't support environment variables yet, so we hardcode the path
const char *PATH[] = {
    "/programs",        // (currently unused)
    "/initrd/programs", // programs in the initrd
    "/initrd/games",    // games in the initrd
    "/initrd/tests",    // userspace tests
    NULL,
};

static const char **tokenize_line(char *line, int *argc)
{
    const char **argv = malloc(sizeof(char *) * 32);
    int i = 0;

    while (*line)
    {
        // skip whitespace
        while (*line == ' ' || *line == '\t')
            line++;

        if (*line == '\0')
            break;

        // handle quotes
        if (*line == '"')
        {
            line++;
            argv[i++] = line;
            while (*line && *line != '"')
                line++;
            if (*line == '"')
                *line++ = '\0';
        }
        else if (*line == '\'')
        {
            line++;
            argv[i++] = line;
            while (*line && *line != '\'')
                line++;
            if (*line == '\'')
                *line++ = '\0';
        }
        else if (*line == '\\')
        {
            line++;
            argv[i++] = line;
            while (*line && *line != '\\')
                line++;
            if (*line == '\\')
                *line++ = '\0';
        }
        else
        {
            argv[i++] = line;
            while (*line && *line != ' ' && *line != '\t')
                line++;
            if (*line == ' ' || *line == '\t')
                *line++ = '\0';
        }
    }

    argv[i] = NULL;
    *argc = i;
    return argv;
}

bool do_program(const char *prog, int argc, const char **argv)
{
    prog = locate_program(prog);
    if (!prog)
        return false;

    pid_t pid = syscall_spawn(prog, argc, argv);
    if (pid == -1)
    {
        fprintf(stderr, "Failed to execute '%s'\n", prog);
        return true;
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

    // if the command ends with / and such directory exists, cd into it

    bool may_be_directory = command[strlen(command) - 1] == '/';
    may_be_directory |= strcmp(command, "..") == 0;
    may_be_directory |= command[0] == '.' && command[1] == '.';
    may_be_directory |= command[0] == '/';
    may_be_directory |= command[0] == '.' && command[1] == '/';

    if (may_be_directory)
    {
        file_stat_t statbuf = { 0 };
        if (stat(command, &statbuf))
        {
            if (statbuf.type == FILE_TYPE_DIRECTORY)
            {
                syscall_vfs_chdir(command);
                return true;
            }
        }
    }

    return false;
}

void do_execute(const char *prog, char *rest)
{
    int new_argc = 0;
    const char **new_argv = tokenize_line(rest, &new_argc);

    if (!do_builtin(prog, new_argc, new_argv))
        if (!do_program(prog, new_argc, new_argv))
            fprintf(stderr, "'%s' is not recognized as an internal, operable program or batch file.\n", prog);

    free(new_argv);
}

void do_execute_line(char *line)
{
    line = string_trim(line);

    // filter comments
    char *comment = strchr(line, '#');
    if (comment)
        *comment = '\0';

    if (*line == '\0')
        return;

    const char *prog = strtok(line, " ");
    char *rest = line + strlen(prog) + 1; // skip the program name

    // possibly replace the line with an alias
    for (size_t i = 0; i < alias_count; i++)
    {
        if (strcmp(prog, alias_list[i].name) == 0)
        {
            char *line_dup = malloc(strlen(alias_list[i].command + strlen(rest) + 2)); // +2 for space and null terminator
            strcpy(line_dup, alias_list[i].command);
            strcat(line_dup, " ");
            strcat(line_dup, rest);

            rest = line_dup;
            prog = strtok(rest, " ");
            rest += strlen(prog) + 1; // skip the program name

            do_execute(prog, rest);
            free(line_dup);
            return;
        }
    }

    do_execute(prog, rest);
}

int do_interpret_script(const char *path)
{
    const fd_t fd = open(path, OPEN_READ);
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
    { NULL, 'c', ARGPARSE_REQUIRED, "MOS shell script file" },
    { "help", 'h', ARGPARSE_NONE, "Show this help message" },
    { "init", 'i', ARGPARSE_REQUIRED, "The initial script to execute" },
    { "version", 'v', ARGPARSE_NONE, "Show the version" },
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
            case 'i':
                printf("Loading initial script '%s'\n", state.optarg);
                if (do_interpret_script(state.optarg) != 0)
                {
                    printf("Failed to execute '%s'\n", state.optarg);
                    return 1;
                }
                break;
            case 'c': return do_interpret_script(argv[2]);
            case 'v': do_execute_line(strdup("version")); return 0;
            case 'h': argparse_usage(&state, mossh_options, "the MOS shell"); return 0;
            default: argparse_usage(&state, mossh_options, "the MOS shell"); return 1;
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
