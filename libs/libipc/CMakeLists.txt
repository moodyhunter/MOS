# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME libipc
    PUBLIC_INCLUDE_DIRECTORIES
        $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
        $<INSTALL_INTERFACE:include>
    SOURCES
        libipc.cpp
)
