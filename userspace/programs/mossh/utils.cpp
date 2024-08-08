// SPDX-License-Identifier: GPL-3.0-or-later
#include "mossh.hpp"

#include <filesystem>
#include <iostream>
#include <regex>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

std::optional<std::filesystem::path> locate_program(const std::string &command)
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
    if (const auto resolved = try_resolve(command); resolved)
        return resolved;

    for (const auto &path : get_paths())
    {
        const auto fullpath = path / command;
        if (auto resolved = try_resolve(fullpath); resolved)
            return resolved;
    }

    return std::nullopt;
}

std::vector<std::string> shlex(const std::string &command)
{
    std::vector<std::string> result;

    const auto len = command.size();
    bool in_double_quot = false, in_single_quot = false;

    for (auto i = 0u; i < len; i++)
    {
        int start = i;
        if (command[i] == '"')
            in_double_quot = true;
        else if (command[i] == '\'')
            in_single_quot = true;

        int arglen;
        if (in_double_quot)
        {
            i++;
            start++;
            while (i < len && command[i] != '"')
                i++; // skip until the next quote
            if (i < len)
                in_double_quot = false; // if we don't reach the end of the string, we found the closing quote
            arglen = i - start;
            i++;
        }
        else if (in_single_quot)
        {
            i++;
            start++;
            while (i < len && command[i] != '\'')
                i++; // skip until the next quote
            if (i < len)
                in_single_quot = false; // if we don't reach the end of the string, we found the closing quote
            arglen = i - start;
            i++;
        }
        else
        {
            while (i < len && command[i] != ' ')
                i++;
            arglen = i - start;
        }

        std::string arg = command.substr(start, arglen);

        // try expanding the variable
        const auto regex = std::regex("(\\$[a-zA-Z0-9_]+)");
        std::smatch match;
        while (std::regex_search(arg, match, regex))
        {
            const auto varname = match[1].str();
            const auto value = getenv(varname.c_str() + 1); // skip the $

            std::cout << "varname: " << varname << ", value: " << value << std::endl;
            if (value)
                arg = std::regex_replace(arg, regex, value);
            else
                arg = std::regex_replace(arg, regex, "");

            std::cout << "arg: " << arg << std::endl;
        }

        result.push_back(arg);
    }

    return result;
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
