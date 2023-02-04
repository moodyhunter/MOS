# SPDX-License-Identifier: GPL-3.0-or-later

add_library(rpc STATIC rpc_server.c rpc_client.c)
target_include_directories(rpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(rpc PUBLIC mos::libuserspace ipc)
