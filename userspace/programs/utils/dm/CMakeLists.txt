# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(dm main.cpp)

add_to_initrd(TARGET dm /programs/utils)

target_link_libraries(dm PRIVATE librpc::client mos::rpc-protocols)
