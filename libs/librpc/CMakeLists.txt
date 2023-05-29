# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME librpc-client
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include-client
        ${CMAKE_CURRENT_LIST_DIR}/include-common
    RELATIVE_SOURCES
        rpc_client.c
    LINK_LIBRARIES
        mos::libipc
)

add_library(librpc::client ALIAS librpc-client)

add_mos_library(
    NAME librpc-server
    USERSPACE_ONLY  # The current code needs to be reworked to work in kernel space
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include-server
        ${CMAKE_CURRENT_LIST_DIR}/include-common
    RELATIVE_SOURCES
        rpc_server.c
    LINK_LIBRARIES
        mos::libipc
)

add_library(librpc::server ALIAS librpc-server)
