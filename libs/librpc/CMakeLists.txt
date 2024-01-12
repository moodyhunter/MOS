# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME librpc-client
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include-client
        ${CMAKE_CURRENT_LIST_DIR}/include-common
    SOURCES
        rpc_client.c
    LINK_LIBRARIES
        mos::libipc
        mos::nanopb
    DEFINES
        __MOS_RPC_CLIENT__
)

add_library(librpc::client ALIAS librpc-client)
add_library(librpc::client_hosted ALIAS librpc-client_hosted)

add_mos_library(
    NAME librpc-server
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include-server
        ${CMAKE_CURRENT_LIST_DIR}/include-common
    SOURCES
        rpc_server.c
    LINK_LIBRARIES
        mos::libipc
    DEFINES
        __MOS_RPC_SERVER__
)

add_library(librpc::server ALIAS librpc-server)
add_library(librpc::server_hosted ALIAS librpc-server_hosted)
