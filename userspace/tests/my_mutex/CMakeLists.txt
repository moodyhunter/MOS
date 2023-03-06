# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(my_mutex main.c)
setup_userspace_program(my_mutex /tests "My own mutex implementation using futexes")
