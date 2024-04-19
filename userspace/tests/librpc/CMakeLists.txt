# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(rpc-test main.c)
target_link_libraries(rpc-test PRIVATE librpc::server mos::librpc-client)
add_to_initrd(TARGET rpc-test /tests)
