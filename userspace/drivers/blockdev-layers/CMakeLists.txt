# SPDX-License-Identifier: GPL-3.0-or-later

generate_nanopb_proto(
    PROTO_SRCS
    PROTO_HEADERS
    ${CMAKE_SOURCE_DIR}/proto/mos_rpc.proto
    ${CMAKE_SOURCE_DIR}/proto/blockdev.proto
)

add_executable(blockdev-layer-gpt layer-gpt.cpp ${PROTO_SRCS} ${PROTO_HEADERS})
add_to_initrd(TARGET blockdev-layer-gpt /programs)

target_link_libraries(blockdev-layer-gpt PRIVATE librpc::client librpc::server blockdev-manager-lib)
