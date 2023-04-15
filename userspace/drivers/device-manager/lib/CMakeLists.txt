# SPDX-License-Identifier: GPL-3.0-or-later

add_library(device-manager-client STATIC client.c)
target_link_libraries(device-manager-client PRIVATE mos::stdlib)
target_link_libraries(device-manager-client PUBLIC librpc::client)
target_include_directories(device-manager-client PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(device-manager::client ALIAS device-manager-client)
