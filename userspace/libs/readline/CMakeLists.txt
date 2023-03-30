# SPDX-License-Identifier: GPL-3.0-or-later

add_library(readline
    libreadline.c
)

target_include_directories(readline PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(readline PUBLIC mos::stdlib)

add_library(mos::readline ALIAS readline)
