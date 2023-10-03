# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(libc-test main.c)
add_to_initrd(TARGET libc-test "/tests/")
