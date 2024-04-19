# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(pipe-test main.c)

add_to_initrd(TARGET pipe-test /tests)
