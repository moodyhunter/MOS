# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(init main.c parser.c)
setup_userspace_program(init /programs "The init program")
target_link_libraries(init PRIVATE mos::argparse)
