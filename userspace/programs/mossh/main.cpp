// SPDX-License-Identifier: GPL-3.0-or-later

#include "mossh.hpp"

#include <argparse/libargparse.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mos/tasks/signal_types.h>
#include <ranges>
#include <readline/libreadline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

bool verbose = false;

static void sigchld_handler(int signal)
{
    MOS_UNUSED(signal);
    if (verbose)
        printf("collecting zombies...");
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
        if (verbose)
            printf(" %d", pid);
    if (verbose)
        puts(" done.");
}

static void sigint_handler(int signal)
{
    MOS_UNUSED(signal);
}

static pid_t spawn(const std::filesystem::path &path, const std::vector<std::string> &argv)
{
    const char **argv_cstr = new const char *[argv.size() + 1];
    for (size_t i = 0; i < argv.size(); i++)
        argv_cstr[i] = argv[i].c_str();
    argv_cstr[argv.size()] = NULL;

    const char *command = path.c_str();

    pid_t pid = fork();
    if (pid == 0)
    {
        execv(command, (char *const *) argv_cstr);
        exit(-1);
    }

    delete[] argv_cstr;
    return pid;
}

bool do_program(const std::string &command, const std::vector<std::string> &argv, bool should_wait)
{
    const auto path = locate_program(command);
    if (!path)
        return false;

    const pid_t pid = spawn(*path, argv);
    if (pid == -1)
    {
        fprintf(stderr, "Failed to execute '%s'\n", path->c_str());
        return true;
    }

    if (should_wait)
    {
        waitpid(pid, NULL, 0);
        int status = 0;
        if (WIFEXITED(status))
        {
            u32 exit_code = WEXITSTATUS(status);
            if (exit_code != 0)
                printf("command '%s' exited with code %d\n", command.c_str(), exit_code);
        }
        else if (WIFSIGNALED(status))
        {
            int sig = WTERMSIG(status);
            printf("command '%s' was terminated by signal %d (%s)\n", command.c_str(), sig, strsignal(sig));
        }
        else
            printf("command '%s' exited with unknown status %d\n", command.c_str(), status);
    }
    else
    {
        printf("Started '%s' with pid %d\n", command.c_str(), pid);
    }

    return true;
}

bool do_builtin(const std::string &command, const std::vector<std::string> &argv)
{
    auto argv_rest = argv;
    argv_rest.erase(argv_rest.begin());

    for (int i = 0; builtin_commands[i].command; i++)
    {
        if (command == builtin_commands[i].command)
        {
            builtin_commands[i].action(argv_rest);
            return true;
        }
    }

    // if the command is a directory, run the cd builtin
    struct stat statbuf;
    if (stat(command.c_str(), &statbuf) == 0)
        if (S_ISDIR(statbuf.st_mode))
            return do_builtin("cd", { "cd", command });

    return false;
}

bool do_execute(const std::string &command, const std::vector<std::string> &argv, bool should_wait)
{
    if (aliases.contains(command))
    {
        const auto alias_command = aliases[command];
        auto alias_argv = shlex(alias_command);
        for (const auto &arg : argv | std::views::drop(1)) // skip the command
            alias_argv.push_back(arg);

        return do_execute(alias_argv[0], alias_argv, should_wait);
    }

    if (!do_builtin(command, argv))
    {
        if (!do_program(command, argv, should_wait))
        {
            fprintf(stderr, "'%s' is not recognized as an internal or external command, operable program or batch file.\n", command.c_str());
            return false;
        }
    }

    return true;
}

void do_execute_line(const std::string &in)
{
    auto line = string_trim(in);

    // check if the line ends with an '&' and if so, run it in the background
    bool should_wait = true;

    if (line.back() == '&')
    {
        should_wait = false;
        line.erase(line.size() - 1);
    }

    // filter comments
    if (line.empty())
        return;

    // skip comments
    if (line[0] == '#')
        return;

    // skip empty lines
    if (line.empty())
        return;

    // split the programs
    const auto argv = shlex(line);
    do_execute(argv[0], argv, should_wait);
}

int do_interpret_script(const std::filesystem::path &path)
{
    std::fstream f(path);
    if (!f.is_open())
    {
        std::cerr << "Failed to open '" << path << "'" << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(f, line))
    {
        if (verbose)
            printf("<script>: %s\n", line.c_str());
        line = string_trim(line);
        do_execute_line(line);
    }

    return 0;
}

static const argparse_arg_t mossh_options[] = {
    { NULL, 'c', ARGPARSE_REQUIRED, "MOS shell script file" },
    { "help", 'h', ARGPARSE_NONE, "Show this help message" },
    { "init", 'i', ARGPARSE_REQUIRED, "The initial script to execute" },
    { "verbose", 'V', ARGPARSE_NONE, "Enable verbose output" },
    { "version", 'v', ARGPARSE_NONE, "Show the version" },
    { "jsonrpc", 'j', ARGPARSE_NONE, "Enable JSON-RPC mode" },
    {},
};

int main(int argc, const char **argv)
{
    MOS_UNUSED(argc);

    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    argparse_state_t state;
    argparse_init(&state, argv);

    bool has_initial_script = false;
    bool json_mode = false;

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
            case 'j': json_mode = true; break;
            case 'h': argparse_usage(&state, mossh_options, "the MOS shell"); return 0;
            default: argparse_usage(&state, mossh_options, "the MOS shell"); return 1;
        }
    }

    printf("Welcome to MOS-sh!\n");
    char cwdbuf[1024] = { 0 };

    if (!has_initial_script)
    {
        const std::filesystem::path init_script = "/initrd/assets/init.msh";
        if (do_interpret_script(init_script) != 0)
        {
            std::cout << "Failed to execute '" << init_script << "'" << std::endl;
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

        const auto prompt = cwdbuf + " > "s;
        char *const line = readline(prompt.c_str());
        const std::string line_str = line;
        free(line);

        do_execute_line(line_str);
    }

    return 0;
}
