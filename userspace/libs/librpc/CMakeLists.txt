# SPDX-License-Identifier: GPL-3.0-or-later

add_library(rpc STATIC librpc.c)
target_include_directories(rpc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(rpc PUBLIC mos::libuserspace)

add_executable(rpc_test tests/main.c)
target_link_libraries(rpc_test PRIVATE rpc)

add_initrd_item(TARGET /lib/rpc/ rpc_test)
