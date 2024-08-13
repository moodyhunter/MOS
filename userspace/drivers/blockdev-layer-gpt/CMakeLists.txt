# SPDX-License-Identifier: GPL-3.0-or-later

generate_nanopb_proto(
    PROTO_SRCS
    PROTO_HEADERS
    ${CMAKE_SOURCE_DIR}/proto/mosrpc.proto
    ${CMAKE_SOURCE_DIR}/proto/blockdev.proto
)

add_executable(blockdev-layer-gpt
    main.cpp
    layer-gpt.cpp
    gptdisk.cpp
    ${PROTO_SRCS}
    ${PROTO_HEADERS}
)

target_link_libraries(blockdev-layer-gpt PRIVATE librpc::client librpc::server blockdev-manager-lib)

add_to_initrd(TARGET blockdev-layer-gpt /programs)
