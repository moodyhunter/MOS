# SPDX-License-Identifier: GPL-3.0-or-later

# inputd daemon
add_executable(inputd
    main.cpp
)

target_link_libraries(inputd PRIVATE libsm::sm libipc)
target_include_directories(inputd PRIVATE include)
add_to_initrd(TARGET inputd /programs)

# # syslogd client library
# add_library(libsyslog INTERFACE)
# target_include_directories(libsyslog INTERFACE include)
# target_link_libraries(libsyslog INTERFACE mos::librpc-client)

# inputd kernel module
mos_add_kernel_module(inputd-kmod NAME inputd SOURCES inputd-kmod.cpp)
target_include_directories(inputd-kmod PRIVATE include)
add_to_initrd(MODULE inputd-kmod /modules)
