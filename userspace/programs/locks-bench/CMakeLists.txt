# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(locks-bench main.c)

setup_userspace_program(locks-bench /programs "Benchmarking different locks")
target_link_libraries(locks-bench PRIVATE x86-console::client mos::private_include)
