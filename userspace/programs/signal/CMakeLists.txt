# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(signal main.c)
setup_userspace_program(signal /programs "signal tests")
