# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(ramdisk
    main.cpp
    ramdisk.cpp
    ${PROTO_SRCS}
    ${PROTO_HEADERS}
)

target_link_libraries(ramdisk
    PRIVATE
        blockdev-manager-lib
        librpc::client
        librpc::server
        mos::rpc-protocols
)

add_to_initrd(TARGET ramdisk /drivers)
add_to_initrd(FILE ramdisk.toml /config/bits)
