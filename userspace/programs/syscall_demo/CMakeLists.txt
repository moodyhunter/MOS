# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(syscall-demo main.c)
setup_userspace_program(syscall-demo /programs "The init program")
