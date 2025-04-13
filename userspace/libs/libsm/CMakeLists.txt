# SPDX-License-Identifier: GPL-3.0-or-later

add_library(sm libsm.cpp)
add_library(libsm::sm ALIAS sm)

target_include_directories(sm PUBLIC SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(sm PUBLIC librpc::client mos::rpc-protocols)
