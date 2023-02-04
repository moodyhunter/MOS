# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(init main.c)
setup_userspace_program(init /programs "The init program")
