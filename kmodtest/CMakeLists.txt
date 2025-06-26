# SPDX-License-Identifier: GPL-3.0-or-later

# Add the kernel module test
mos_add_kernel_module(kmodtest
    SOURCES
        kmod1.cpp
        kmod1-impl.cpp
)
