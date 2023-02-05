# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(device-manager main.c)
setup_userspace_program(device-manager /programs "The device manager daemon")
target_link_libraries(device-manager PRIVATE rpc)
