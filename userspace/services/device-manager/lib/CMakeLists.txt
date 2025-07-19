# SPDX-License-Identifier: GPL-3.0-or-later

add_library(device-manager-client INTERFACE)

target_include_directories(device-manager-client
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(device-manager::client ALIAS device-manager-client)
