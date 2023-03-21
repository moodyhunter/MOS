# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME stdlib
    PUBLIC_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    RELATIVE_SOURCES
        liballoc.c
        stdio.c
        stdio_impl.c
        stdlib.c
        string.c
        structures/bitmap.c
        structures/hashmap.c
        structures/hashmap_common.c
        structures/list.c
        structures/ring_buffer.c
        structures/stack.c
        structures/tree.c
        sync/mutex.c
)
