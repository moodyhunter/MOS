// SPDX-License-Identifier: GPL-3.0-or-later

#include "LaunchContext.hpp"
#include "mossh.hpp"
#include "parser.hpp"

#include <argparse/libargparse.h>
#include <cassert>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mos/tasks/signal_types.h>
#include <readline/libreadline.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define C_BLUE "\033[1;34m"

bool verbose = false;

bool execute_line(const std::string &in)
{
    auto line = string_trim(in);

    // split the programs
    auto spec = parse_commandline(line);
    if (!spec)
        return false;

    LaunchContext ctx{ std::move(spec) };
    return ctx.start();
}

static void sigchld_handler(int signal)
{
    MOS_UNUSED(signal);
    if (verbose)
        printf("collecting zombies...");
    pid_t pid;
    int status = 0;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        if (verbose)
            printf(" %d", pid);

        if (WIFEXITED(status))
        {
            if (verbose && WEXITSTATUS(status) != 0)
                printf(" exited with status %d", WEXITSTATUS(status));
        }
        else if (WIFSIGNALED(status))
        {
            if (verbose)
                printf(" killed by signal %d", WTERMSIG(status));
        }
        else if (WIFSTOPPED(status))
        {
            if (verbose)
                printf(" stopped by signal %d", WSTOPSIG(status));
        }
        else if (WIFCONTINUED(status))
        {
            if (verbose)
                printf(" continued");
        }
    }
    if (verbose)
        puts(" done.");
}

static void sigint_handler(int signal)
{
    MOS_UNUSED(signal);
}

bool do_interpret_script(const std::filesystem::path &path)
{
    std::fstream f(path);
    if (!f.is_open())
    {
        std::cerr << "Failed to open '" << path << "'" << std::endl;
        return false;
    }

    std::string line;
    while (std::getline(f, line))
    {
        if (verbose)
            std::cout << "<script>: " << line << std::endl;
        execute_line(line);
    }

    return true;
}

static const argparse_arg_t mossh_options[] = {
    { NULL, 'c', ARGPARSE_REQUIRED, "MOS shell script file" },              //
    { "help", 'h', ARGPARSE_NONE, "Show this help message" },               //
    { "init", 'i', ARGPARSE_REQUIRED, "The initial script to execute" },    //
    { "no-init", 'I', ARGPARSE_NONE, "Do not execute the initial script" }, //
    { "verbose", 'V', ARGPARSE_NONE, "Enable verbose output" },             //
    { "version", 'v', ARGPARSE_NONE, "Show the version" },                  //
    { "jsonrpc", 'j', ARGPARSE_NONE, "Enable JSON-RPC mode" },              //
    {},                                                                     //
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

    std::filesystem::path init_script = "/initrd/assets/init.msh";

    bool json_mode = false;

    while (true)
    {
        const int option = argparse_long(&state, mossh_options, NULL);
        if (option == -1)
            break;

        switch (option)
        {
            case 'i': init_script = state.optarg; break;
            case 'I': init_script.clear(); break;
            case 'c': return do_interpret_script(argv[2]) ? 0 : 1;
            case 'V': verbose = true; break;
            case 'v': execute_line(strdup("version")); return 0;
            case 'j': json_mode = true; break;
            case 'h': argparse_usage(&state, mossh_options, "the MOS shell"); return 0;
            default: argparse_usage(&state, mossh_options, "the MOS shell"); return 1;
        }
    }

    if (!init_script.empty())
    {
        if (!do_interpret_script(init_script))
        {
            std::cout << "Failed to execute '" << init_script << "'" << std::endl;
            return 1;
        }
    }

    std::cout << "Welcome to MOS-sh!" << std::endl;

    if (json_mode)
    {
        std::cout << "JSON-RPC mode enabled." << std::endl;
        return do_jsonrpc();
    }

    char cwdbuf[1024] = { 0 };

    while (1)
    {
        if (!getcwd(cwdbuf, sizeof(cwdbuf)))
        {
            std::cerr << "Failed to get current working directory." << std::endl;
            cwdbuf[0] = '?';
            cwdbuf[1] = '\0';
        }

        const auto prompt = std::string{ C_BLUE } + cwdbuf + " > "s;
        char *const line = readline(prompt.c_str());
        const std::string line_str = line;
        free(line);
        execute_line(line_str);
    }

    return 0;
}
