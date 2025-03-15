# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(sc main.cpp)

add_to_initrd(TARGET sc /programs)

target_link_libraries(sc PRIVATE librpc::client mos::rpc-protocols)
