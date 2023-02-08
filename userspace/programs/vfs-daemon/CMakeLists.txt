# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(vfs-daemon main.c)
setup_userspace_program(vfs-daemon /programs "Virtual File System Daemon")
target_link_libraries(vfs-daemon PRIVATE rpc)
