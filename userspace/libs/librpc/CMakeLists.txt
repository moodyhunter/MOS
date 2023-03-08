# SPDX-License-Identifier: GPL-3.0-or-later

add_library(rpc-client STATIC rpc_client.c)
target_include_directories(rpc-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(rpc-client PUBLIC mos::libuserspace ipc)

add_library(rpc-server STATIC rpc_server.c)
target_include_directories(rpc-server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(rpc-server PUBLIC mos::libuserspace ipc)

add_library(rpc INTERFACE)
target_link_libraries(rpc INTERFACE rpc-client rpc-server)
