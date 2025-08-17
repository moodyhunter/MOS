# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME librpc-client
    PUBLIC_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include-client>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include-common>
    SOURCES
        rpc_client.cpp
    LINK_LIBRARIES
        mos::libipc
        mos::nanopb
    PUBLIC_HEADER_SET
        include-client/librpc/rpc_client.h
    PUBLIC_HEADER_SET_BASE
        include-client/
    DEFINES
        __MOS_RPC_CLIENT__
)

install(
    DIRECTORY include-common/
    COMPONENT sdk
    EXCLUDE_FROM_ALL
    DESTINATION include/librpc-client
    FILES_MATCHING PATTERN "*.h"
)

add_library(librpc::client ALIAS librpc-client)

add_mos_library(
    NAME librpc-server
    PUBLIC_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include-server>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include-common>
    SOURCES
        rpc_server.cpp
    LINK_LIBRARIES
        mos::libipc
        mos::nanopb
    PUBLIC_HEADER_SET
        include-server/librpc/rpc_server.h
        include-server/librpc/rpc_server++.hpp
    PUBLIC_HEADER_SET_BASE
        include-server/
    DEFINES
        __MOS_RPC_SERVER__
)

install(
    DIRECTORY include-common/
    COMPONENT sdk
    EXCLUDE_FROM_ALL
    DESTINATION include/librpc-server
    FILES_MATCHING PATTERN "*.h"
)

add_library(librpc::server ALIAS librpc-server)
