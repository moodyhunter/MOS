# SPDX-License-Identifier: GPL-3.0-or-later

add_userspace_program(kmsg-ping SOURCES ping.c)
add_userspace_program(kmsg-pong SOURCES pong.c)

add_initrd_item(TARGET /programs kmsg-ping)
add_initrd_item(TARGET /programs kmsg-pong)
