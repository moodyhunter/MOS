# SPDX-License-Identifier: GPL-3.0-or-later

generate_nanopb_proto(
    PROTO_SRCS
    PROTO_HEADERS
    ${CMAKE_SOURCE_DIR}/proto/mosrpc.proto
    ${CMAKE_SOURCE_DIR}/proto/blockdev.proto
    ${CMAKE_SOURCE_DIR}/proto/filesystem.proto
)

add_library(blockdev-manager-lib INTERFACE)
target_include_directories(blockdev-manager-lib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(blockdev-manager
    main.cpp
    blockdev_manager.cpp
    blockdevfs.cpp
    ${PROTO_SRCS}
    ${PROTO_HEADERS}
)

target_link_libraries(blockdev-manager
    PRIVATE
        blockdev-manager-lib
        librpc::client
        librpc::server
)

add_to_initrd(TARGET blockdev-manager /drivers)
