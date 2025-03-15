// SPDX-License-Identifier: GPL-3.0-or-later

#include "LaunchContext.hpp"

#include "mossh.hpp"
#include "parser.hpp"

#include <bits/posix/posix_string.h>
#include <cassert>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <ranges>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const auto find_builtin = [](const std::string &name) -> std::optional<command_t>
{
    for (const auto &builtin : builtin_commands)
        if (builtin.command == name)
            return builtin;
    return std::nullopt;
};

static const auto find_alias = [](const std::string &name) -> std::optional<std::string>
{
    if (aliases.contains(name))
        return aliases[name];
    return std::nullopt;
};

static std::optional<std::filesystem::path> locate_program(const std::string &command)
{
    const auto try_resolve = [&](const std::filesystem::path &path) -> std::optional<std::filesystem::path>
    {
        struct stat statbuf;
        if (stat(path.c_str(), &statbuf) == 0)
            if (S_ISREG(statbuf.st_mode))
                return path;
        return std::nullopt;
    };

    // firstly try the command as is
    if (command.starts_with('/') || command.starts_with("./"))
        return try_resolve(command);

    for (const auto &path : get_paths())
    {
        const auto fullpath = path / command;
        if (auto resolved = try_resolve(fullpath); resolved)
            return resolved;
    }

    return std::nullopt;
}

bool FDRedirection::do_redirect(int fd)
{
    if (target_fd == -1)
    {
        std::cerr << "FDRedirection: target_fd is not set" << std::endl;
        return false;
    }

    if (verbose)
        std::cout << "Redirecting fd " << fd << " to fd " << target_fd << std::endl;

    if (dup2(target_fd, fd) == -1)
    {
        std::cerr << "Failed to redirect fd " << fd << " to fd " << target_fd << std::endl;
        return false;
    }

    return true;
}

bool FileRedirection::do_redirect(int fd)
{
    if (file.empty())
    {
        std::cerr << "FileRedirection: file is not set" << std::endl;
        return false;
    }

    if (verbose)
        std::cout << "Redirecting fd " << fd << " to file " << file << std::endl;

    int flags = O_CREAT;

    if (mode == ReadOnly)
        flags |= O_RDONLY;
    else if (mode == ReadWrite)
        flags |= O_RDWR;
    else if (mode == WriteOnly)
        flags |= O_WRONLY;

    if (append)
        flags |= O_APPEND;

    const auto file_fd = open(file.c_str(), flags, 0644);
    if (file_fd == -1)
    {
        std::cerr << "Failed to open file " << file << "(" << strerror(errno) << ")" << std::endl;
        return false;
    }

    if (dup2(file_fd, fd) == -1)
    {
        std::cerr << "Failed to redirect fd " << fd << " to file " << file << std::endl;
        return false;
    }

    // close the file descriptor
    if (file_fd != fd)
        close(file_fd);

    return true;
}

LaunchContext::LaunchContext(std::unique_ptr<ProgramSpec> &&spec) : program_spec(std::move(spec))
{
    assert(program_spec);
    argv = program_spec->argv;
    redirections = std::move(program_spec->redirections);

    // get need_wait
    should_wait = !program_spec->background;
}

LaunchContext::LaunchContext(const std::vector<std::string> &argv) : argv(argv)
{
}

bool LaunchContext::resolve_program_path()
{
    if (m_program_path.empty())
    {
        if (const auto resolved = locate_program(command()); resolved)
        {
            m_program_path = *resolved;
            return true;
        }
        return false;
    }
    return true;
}

bool LaunchContext::try_start_builtin() const
{
    if (const auto builtin = find_builtin(command()); builtin)
    {
        builtin->action(argv | std::views::drop(1) | std::ranges::to<std::vector>());
        return true;
    }

    // if the command is a directory, run the cd builtin
    struct stat statbuf;
    if (stat(command().c_str(), &statbuf))
        return false;

    if (S_ISDIR(statbuf.st_mode))
    {
        static const auto cd_builtin = find_builtin("cd");
        assert(cd_builtin);
        cd_builtin->action({ command() });
        return true;
    }

    return false;
}

bool LaunchContext::try_start_alias() const
{
    if (const auto alias = find_alias(command()); alias)
    {
        auto alias_spec = parse_commandline(*alias);
        for (const auto &arg : argv | std::views::drop(1))
            alias_spec->argv.push_back(arg);

        LaunchContext alias_context{ std::move(alias_spec) };
        return alias_context.start();
    }

    return false;
}

bool LaunchContext::try_start_program()
{
    const pid_t pid = fork();
    if (pid == -1)
    {
        fprintf(stderr, "Failed to execute '%s'\n", program_path().c_str());
        return false;
    }

    if (pid == 0)
    {
        spawn_in_child();
        __builtin_unreachable();
    }

    if (should_wait)
    {
        std::tie(exit_code, exit_signal) = wait_for_pid(pid);

        if (exit_code != 0)
        {
            std::cerr << "Program '" << command() << "' exited with code " << exit_code << std::endl;
            return true;
        }
        else if (exit_signal != 0)
        {
            std::cerr << "Program '" << command() << "' exited with signal " << exit_signal << std::endl;
            return true;
        }
    }
    else
    {
        std::cout << "Started '" << argv[0] << "' with pid " << pid << std::endl;
    }

    return true;
}

bool LaunchContext::spawn_in_child() const
{
    const auto argc = argv.size();

    const char **argv_cstr = new const char *[argc + 1];
    for (size_t i = 0; i < argc; i++)
        argv_cstr[i] = argv[i].c_str();
    argv_cstr[argc] = NULL;

    // perform redirections
    for (const auto &[fd, redir] : redirections)
    {
        if (redir->do_redirect(fd))
            continue;

        fprintf(stderr, "Failed to redirect fd %d\n", fd);
        goto bail;
    }

    execv(m_program_path.c_str(), (char *const *) argv_cstr);

bail:
    fprintf(stderr, "Failed to execute '%s'\n", m_program_path.c_str());
    delete[] argv_cstr; // actually, unreachable
    _exit(-1);
}

bool LaunchContext::start()
{
    bool success = false;
    if (!success && launch_type.builtin)
        success = try_start_builtin();

    if (!success && launch_type.alias)
        success = try_start_alias();

    if (!success && launch_type.program)
    {
        if (!resolve_program_path())
            success = false;
        else
            success = try_start_program();
    }

    if (!success)
        fprintf(stderr, "'%s' is not recognized as an internal or external command, operable program or batch file.\n", command().c_str());

    return success;
}
