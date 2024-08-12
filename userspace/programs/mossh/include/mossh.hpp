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

extern const std::vector<command_t> builtin_commands;
extern bool verbose;
extern std::map<std::string, std::string> aliases;

bool execute_line(const std::string &in);
bool do_interpret_script(const std::filesystem::path &path);

// jsonrpc.cpp
int do_jsonrpc();

// utils.cpp
const std::vector<std::filesystem::path> &get_paths(bool force = false);
std::string string_trim(const std::string &in);
std::pair<int, int> wait_for_pid(pid_t pid, int flags = 0);
