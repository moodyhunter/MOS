# SPDX-License-Identifier: GPL-3.0-or-later

generate_nanopb_proto(PROTO_SRCS PROTO_HDRS
    ${CMAKE_SOURCE_DIR}/proto/mos_rpc.proto
    ${CMAKE_SOURCE_DIR}/proto/filesystem.proto
)

add_executable(bootstrapper
    main.c
    cpiofs.c
    cpiofs_server.c
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(bootstrapper PRIVATE
    mos::include
    mos::nanopb_static
    mos::librpc-server_static
    mos::librpc-client_static
)
target_compile_options(bootstrapper PRIVATE -static)
target_link_options(bootstrapper PRIVATE -static)
add_to_initrd(TARGET bootstrapper /programs)
