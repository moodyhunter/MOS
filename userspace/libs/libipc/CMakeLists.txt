# SPDX-License-Identifier: GPL-3.0-or-later
add_library(ipc STATIC libipc.c)
target_include_directories(ipc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ipc PUBLIC mos::libuserspace)
