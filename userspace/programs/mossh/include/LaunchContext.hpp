// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "parser.hpp"

#include <cassert>
#include <ctime>
#include <filesystem>
#include <map>
#include <memory>
#include <vector>

struct LaunchContext
{
    explicit LaunchContext(std::unique_ptr<ProgramSpec> &&program_spec);

    explicit LaunchContext(const std::vector<std::string> &argv);

    /**
     * @brief Get the command name, i.e. the first argument as argv[0]
     *
     * @return std::string
     */
    std::string command() const
    {
        assert(!argv.empty());
        return argv[0];
    }

    void redirect(int fd, std::unique_ptr<BaseRedirection> &&redirection)
    {
        redirections[fd] = std::move(redirection);
    }

    std::filesystem::path program_path() const
    {
        return m_program_path;
    }

    // resolve the program path
    bool resolve_program_path();

    // start the program (or builtin, or alias)
    bool start();

    std::vector<std::string> argv; //< the arguments (including the command), e.g. {"ls", "-l", "-a"}

    bool should_wait = true;
    int exit_code = 0;
    int exit_signal = 0;
    bool success = false;

    std::map<int, std::unique_ptr<BaseRedirection>> redirections; // fd -> redirection

    struct
    {
        bool builtin, program, alias;
    } launch_type = { true, true, true };

  private:
    bool try_start_alias();
    bool try_start_builtin();
    bool try_start_program();

    bool spawn_in_child() const;

  private:
    std::unique_ptr<ProgramSpec> program_spec;
    std::filesystem::path m_program_path; //< the path to the program that to be executed, e.g. "/bin/ls"
};
