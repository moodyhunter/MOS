# SPDX-License-Identifier: GPL-3.0-or-later

generate_nanopb_proto(
    PROTO_SRCS
    PROTO_HEADERS
    ${CMAKE_SOURCE_DIR}/proto/mosrpc.proto
    ${CMAKE_SOURCE_DIR}/proto/blockdev.proto
)

add_executable(peekblock
    peekblock.cpp
    ${PROTO_SRCS}
    ${PROTO_HEADERS}
)

add_to_initrd(TARGET peekblock /programs)

target_link_libraries(peekblock PRIVATE librpc::client nanopb blockdev-manager-lib)
