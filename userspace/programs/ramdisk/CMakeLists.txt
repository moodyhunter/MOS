# SPDX-License-Identifier: GPL-3.0-or-later

generate_nanopb_proto(
    PROTO_SRCS
    PROTO_HEADERS
    ${CMAKE_SOURCE_DIR}/proto/mos_rpc.proto
    ${CMAKE_SOURCE_DIR}/proto/blockdev.proto
)

add_executable(ramdisk
    main.cpp
    ramdisk.cpp
    ${PROTO_SRCS}
    ${PROTO_HEADERS}
)

target_link_libraries(ramdisk
    PRIVATE
        blockdev-manager-lib
        nanopb
        librpc::client
        librpc::server
)

add_to_initrd(TARGET ramdisk /programs)
