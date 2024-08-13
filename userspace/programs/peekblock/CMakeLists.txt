# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(peekblock
    peekblock.cpp
    ${PROTO_SRCS}
    ${PROTO_HEADERS}
)

add_to_initrd(TARGET peekblock /programs)

target_link_libraries(peekblock PRIVATE librpc::client nanopb blockdev-manager-lib mos::rpc-protocols)
