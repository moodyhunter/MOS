# SPDX-License-Identifier: GPL-3.0-or-later
add_executable(blockdev-layer-gpt
    main.cpp
    layer-gpt.cpp
    gptdisk.cpp
    ${PROTO_SRCS}
    ${PROTO_HEADERS}
)

target_link_libraries(blockdev-layer-gpt PRIVATE mos::rpc-protocols librpc::client librpc::server blockdev-manager-lib)

add_to_initrd(TARGET blockdev-layer-gpt /services)
