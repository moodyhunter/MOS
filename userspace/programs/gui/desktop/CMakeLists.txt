# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(desktop main.cpp)
target_link_libraries(desktop
    PRIVATE
    librpc::client
    mos::rpc-protocols
    libsm::sm
)
add_to_initrd(TARGET desktop /programs)
