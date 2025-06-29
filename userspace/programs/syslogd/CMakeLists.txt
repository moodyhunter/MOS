# SPDX-License-Identifier: GPL-3.0-or-later
# Add the kernel module test

# syslogd daemon
add_executable(syslogd
    main.cpp
)

target_link_libraries(syslogd PRIVATE libsm::sm libipc)
target_include_directories(syslogd PRIVATE include)
add_to_initrd(TARGET syslogd /programs)

# syslogd client library
add_library(libsyslog INTERFACE)
target_include_directories(libsyslog INTERFACE include)
target_link_libraries(libsyslog INTERFACE mos::librpc-client)

# syslogd kernel module
mos_add_kernel_module(syslogd-kmod NAME syslogd SOURCES kmod.cpp)
target_include_directories(syslogd-kmod PRIVATE include)
add_to_initrd(MODULE syslogd-kmod /modules)
