// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <map>
#include <stddef.h>
#include <vector>

using namespace std::string_literals;

void do_help(int argc, const char *argv[]);
void do_version(int argc, const char *argv[]);
void do_exit(int argc, const char *argv[]);
void do_clear(int argc, const char *argv[]);

typedef struct
{
    const char *command;
    void (*action)(const std::vector<std::string> &argv);
    const char *description;
} command_t;

extern const command_t builtin_commands[];
extern const char *PATH[];
extern bool verbose;
extern std::map<std::string, std::string> aliases;

std::optional<std::filesystem::path> locate_program(const std::string &command);

bool do_execute(const std::string &command, const std::vector<std::string> &argv, bool should_wait);

int do_interpret_script(const std::filesystem::path &path);

// utils.cpp
const std::vector<std::filesystem::path> &get_paths(bool force = false);
std::vector<std::string> shlex(const std::string &command);
std::string string_trim(const std::string &in);
