# SPDX-License-Identifier: GPL-3.0-or-later

generate_nanopb_proto(PROTO_SRCS PROTO_HDRS ${CMAKE_SOURCE_DIR}/proto/filesystem.proto)

add_executable(bootstrapper
    main.c
    cpiofs.c
    cpiofs_server.c
    ${PROTO_SRCS}
    ${PROTO_HDRS}
)

target_link_libraries(bootstrapper PRIVATE
    mos::include
    mos::nanopb
    mos::stdlib
    mos::librpc-server
    mos::librpc-client
)
target_compile_options(bootstrapper PRIVATE -static)
target_link_options(bootstrapper PRIVATE -static)
add_to_initrd(TARGET bootstrapper /programs)

