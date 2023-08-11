# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(kdebug main.c)
target_link_libraries(kdebug PRIVATE mos::readline)

setup_userspace_program(kdebug /programs "Debug kernel parameters")
