# SPDX-License-Identifier: GPL-3.0-or-later

set(LIB_ONLY ON)
add_subdirectory(lwext4)

generate_nanopb_proto(PROTO_SRCS PROTO_HDRS
    ${CMAKE_SOURCE_DIR}/proto/mos_rpc.proto
    ${CMAKE_SOURCE_DIR}/proto/filesystem.proto
    ${CMAKE_SOURCE_DIR}/proto/blockdev.proto
)

add_executable(ext4fs
    main.cpp
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(ext4fs PRIVATE lwext4 blockdev-manager-lib)
target_include_directories(ext4fs PRIVATE lwext4/include)
target_compile_definitions(ext4fs PRIVATE -DCONFIG_USE_DEFAULT_CFG=1)

target_link_libraries(ext4fs PRIVATE
    mos::include
    mos::nanopb
    mos::librpc-server
    mos::librpc-client
)

add_to_initrd(TARGET ext4fs /drivers/)
