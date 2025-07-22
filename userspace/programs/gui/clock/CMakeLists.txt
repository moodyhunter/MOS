# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(clock main.cpp)
target_link_libraries(clock
    PRIVATE
    librpc::client
    mos::rpc-protocols
)
add_to_initrd(TARGET clock /programs)
