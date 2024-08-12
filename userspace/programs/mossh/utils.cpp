// SPDX-License-Identifier: GPL-3.0-or-later
#include "mossh.hpp"

#include <filesystem>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <vector>

const std::vector<std::filesystem::path> &get_paths(bool force)
{
    static std::vector<std::filesystem::path> paths;
    static bool paths_initialized = false;

    if (paths_initialized && !force)
        return paths;

    paths.clear();
    const char *path = getenv("PATH");
    if (!path)
        paths_initialized = true;

    std::string p = path;
    size_t start = 0;
    size_t end = p.find(':');
    while (end != std::string::npos)
    {
        paths.push_back(p.substr(start, end - start));
        start = end + 1;
        end = p.find(':', start);
    }

    paths.push_back(p.substr(start));
    paths_initialized = true;
    return paths;
}

std::string string_trim(const std::string &in)
{
    std::string out = in;

    // trim trailing spaces, tabs, and newlines
    out.erase(out.find_last_not_of(" \n\r\t") + 1);

    // trim leading spaces, tabs, and newlines
    size_t startpos = out.find_first_not_of(" \n\r\t");
    if (std::string::npos != startpos)
        out = out.substr(startpos);

    return out;
}

std::pair<int, int> wait_for_pid(pid_t pid, int flags)
{
    std::pair<int, int> exit_code_signal;

    int status = 0;
    waitpid(pid, &status, flags);
    if (WIFEXITED(status))
    {
        exit_code_signal.first = WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        exit_code_signal.second = WTERMSIG(status);
    }

    return exit_code_signal;
}
