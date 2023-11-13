// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>

void do_help(int argc, const char *argv[]);
void do_version(int argc, const char *argv[]);
void do_exit(int argc, const char *argv[]);
void do_clear(int argc, const char *argv[]);

typedef struct
{
    const char *command;
    void (*action)(int argc, const char *argv[]);
    const char *description;
} command_t;

typedef struct
{
    char *name;
    char *command;
} alias_t;
extern alias_t *alias_list;
extern size_t alias_count;

extern const command_t builtin_commands[];
extern const char *PATH[];

const char *locate_program(const char *prog);

bool do_builtin(const char *command, int argc, const char **argv);
bool do_program(const char *command, int argc, const char **argv, bool should_wait);
int do_interpret_script(const char *path);
