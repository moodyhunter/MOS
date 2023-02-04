# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(fork-test main.c)
setup_userspace_program(fork-test /tests "Kernel fork() test")
