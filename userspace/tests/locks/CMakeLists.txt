# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(locks-test main.c)
setup_userspace_program(locks-test /tests "Tests for different locks")
