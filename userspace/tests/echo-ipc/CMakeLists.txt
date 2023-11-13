# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(ipc-echo-server main.c)
target_link_libraries(ipc-echo-server PRIVATE mos::include mos::libipc)
add_to_initrd(TARGET ipc-echo-server /tests)
