# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(ipc-ping ping.c)
add_executable(ipc-pong pong.c)

add_to_initrd(TARGET ipc-ping /tests)
add_to_initrd(TARGET ipc-pong /tests)
