# SPDX-License-Identifier: GPL-3.0-or-later

add_library(argparse libargparse.c)
target_include_directories(argparse PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(argparse PUBLIC mos::stdlib)
add_library(mos::argparse ALIAS argparse)
