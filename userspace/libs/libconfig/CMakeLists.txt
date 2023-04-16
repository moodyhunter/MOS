# SPDX-License-Identifier: GPL-3.0-or-later

add_library(libconfig STATIC libconfig.c)
target_include_directories(libconfig PUBLIC SYSTEM ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(libconfig PUBLIC mos::stdlib)
add_library(mos::libconfig ALIAS libconfig)
