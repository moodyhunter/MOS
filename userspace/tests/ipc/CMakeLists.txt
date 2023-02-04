# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(ipc-ping ping.c)
add_executable(ipc-pong pong.c)

setup_userspace_program(ipc-ping /tests "Kernel IPC test - Ping Server")
setup_userspace_program(ipc-pong /tests "Kernel IPC test - Pong Client")
