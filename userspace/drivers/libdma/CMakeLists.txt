# SPDX-License-Identifier: GPL-3.0-or-later

add_mos_library(
    NAME dma
    SOURCES libdma.c
    USERSPACE_ONLY
)

add_to_initrd(TARGET dma /lib/)
