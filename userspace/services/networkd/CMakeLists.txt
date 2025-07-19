# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(networkd
    main.cpp
)

target_link_libraries(networkd
PRIVATE
    librpc::client
    librpc::server
    libsm::sm
)

add_to_initrd(TARGET networkd /services)
add_to_initrd(FILE networkd.toml /config/bits)
