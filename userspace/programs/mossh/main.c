// SPDX-License-Identifier: GPL-3.0-or-later

#include "argparse/libargparse.h"
#include "mossh.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MOS_UNUSED(x) (void) x

extern const char **cmdline_parse(const char **inargv, char *inbuf, size_t length, size_t *out_count);
extern void string_unquote(char *str);

// We don't support environment variables yet, so we hardcode the path
const char *PATH[] = {
    "/programs",        // (currently unused)
    "/initrd/bin",      // programs in the initrd
    "/initrd/programs", // programs in the initrd
    "/initrd/drivers",  // builtin drivers
    "/initrd/games",    // games in the initrd
    "/initrd/tests",    // userspace tests
    NULL,
};

static bool verbose = false;

static char *string_trim(char *in)
{
    if (in == NULL)
        return NULL;

    char *end;

    // Trim leading space
    while (*in == ' ')
        in++;

    if (*in == 0) // All spaces?
        return in;

    // Trim trailing space
    end = in + strlen(in) - 1;
    while (end > in && *end == ' ')
        end--;

    // Trim trailing newline
    if (*end == '\n')
        end--;

    // Write new null terminator
    *(end + 1) = '\0';
    return in;
}

char *readline(const char *prompt)
{
    char *line = NULL;
    size_t linecap = 0;
    printf("%s", prompt);
    fflush(stdout);
    ssize_t linelen = getline(&line, &linecap, stdin);
    if (linelen <= 0)
    {
        free(line);
        return NULL;
    }
    line[linelen - 1] = '\0'; // remove newline
    return line;
}

static pid_t spawn(const char *path, const char *const argv[])
{
    pid_t pid = fork();
    if (pid == 0)
    {
        execve(path, (char *const *) argv, environ);
        exit(-1);
    }

    return pid;
}

bool do_program(const char *prog, int argc, const char **argv)
{
    prog = locate_program(prog);
    if (!prog)
        return false;

    MOS_UNUSED(argc);
    pid_t pid = spawn(prog, argv);
    if (pid == -1)
    {
        fprintf(stderr, "Failed to execute '%s'\n", prog);
        return true;
    }

    waitpid(pid, NULL, 0);
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
        struct stat statbuf = { 0 };
        if (!stat(command, &statbuf))
        {
            if (S_ISDIR(statbuf.st_mode))
            {
                chdir(command);
                return true;
            }
        }
    }

    return false;
}

void do_execute(const char *prog, char *rest)
{
    size_t argc = 1;
    const char **argv = malloc(sizeof(char *));
    argv[0] = prog;
    argv = cmdline_parse(argv, rest, strlen(rest), &argc);

    for (size_t i = 1; i < argc; i++)
        string_unquote((char *) argv[i]);

    argv = realloc(argv, sizeof(char *) * (argc + 1));
    argv[argc] = NULL;

    if (!do_builtin(prog, argc - 1, argv + 1))
        if (!do_program(prog, argc, argv))
            fprintf(stderr, "'%s' is not recognized as an internal, operable program or batch file.\n", prog);

    if (argc)
    {
        for (size_t i = 1; i < argc; i++)
            free((void *) argv[i]);
        free(argv);
    }
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
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "Failed to open '%s'\n", path);
        return 1;
    }

    char linebuf[1024] = { 0 };

    do
    {
        char *line = fgets(linebuf, sizeof(linebuf), f);
        if (line)
        {
            if (verbose)
                printf("<script>: %s\n", line);
            line = string_trim(line);
            do_execute_line(line);
        }
    } while (!feof(f));

    fclose(f);
    return 0;
}

static const argparse_arg_t mossh_options[] = {
    { NULL, 'c', ARGPARSE_REQUIRED, "MOS shell script file" },
    { "help", 'h', ARGPARSE_NONE, "Show this help message" },
    { "init", 'i', ARGPARSE_REQUIRED, "The initial script to execute" },
    { "verbose", 'V', ARGPARSE_NONE, "Enable verbose output" },
    { "version", 'v', ARGPARSE_NONE, "Show the version" },
    { 0 },
};

int main(int argc, const char **argv)
{
    MOS_UNUSED(argc);

    argparse_state_t state;
    argparse_init(&state, argv);

    bool has_initial_script = false;

    while (true)
    {
        const int option = argparse_long(&state, mossh_options, NULL);
        if (option == -1)
            break;

        switch (option)
        {
            case 'i':
                printf("Loading initial script '%s'\n", state.optarg);
                has_initial_script = true;
                if (do_interpret_script(state.optarg) != 0)
                {
                    printf("Failed to execute '%s'\n", state.optarg);
                    return 1;
                }
                break;
            case 'c': return do_interpret_script(argv[2]);
            case 'V': verbose = true; break;
            case 'v': do_execute_line(strdup("version")); return 0;
            case 'h': argparse_usage(&state, mossh_options, "the MOS shell"); return 0;
            default: argparse_usage(&state, mossh_options, "the MOS shell"); return 1;
        }
    }

    printf("Welcome to MOS-sh!\n");
    char cwdbuf[1024] = { 0 };

    if (!has_initial_script)
    {
        const char *init_script = "/initrd/assets/init.msh";
        if (do_interpret_script(init_script) != 0)
        {
            printf("Failed to execute '%s'\n", init_script);
            return 1;
        }
    }

    while (1)
    {
        if (!getcwd(cwdbuf, sizeof(cwdbuf)))
        {
            fputs("Failed to get current working directory.\n", stderr);
            cwdbuf[0] = '?';
            cwdbuf[1] = '\0';
        }

        const char *prompt_part2 = " > ";
        const size_t prompt_len = strlen(cwdbuf) + strlen(prompt_part2) + 1;
        char *prompt = malloc(prompt_len);
        memset(prompt, 0, prompt_len);
        sprintf((char *) prompt, "%s%s", cwdbuf, prompt_part2);

        char *line = readline(prompt);
        do_execute_line(line);
        free(line);
        free((void *) prompt);
    }

    return 0;
}
