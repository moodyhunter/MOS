# SPDX-License-Identifier: GPL-3.0-or-later

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
        mos::rpc-protocols
        libsm::sm
)

add_to_initrd(TARGET blockdev-manager /services)
add_to_initrd(FILE blockdev-manager.toml /config/bits)
