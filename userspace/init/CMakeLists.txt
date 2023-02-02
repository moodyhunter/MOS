# SPDX-License-Identifier: GPL-3.0-or-later

add_userspace_program(init SOURCES main.c)
add_initrd_item(TARGET /programs init)
