# SPDX-License-Identifier: GPL-3.0-or-later

add_library(x86_console-client STATIC client.c)
target_link_libraries(x86_console-client PRIVATE mos::stdlib)
target_link_libraries(x86_console-client PUBLIC librpc::client)
target_include_directories(x86_console-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(x86-console::client ALIAS x86_console-client)
