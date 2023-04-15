# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(my_mutex main.c)

setup_userspace_program(my_mutex /programs "My own mutex implementation")
target_link_libraries(my_mutex PRIVATE x86-console::client)
