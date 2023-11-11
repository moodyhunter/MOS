# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(pipe-test main.c)

setup_userspace_program(pipe-test /tests "Pipe test")
