# SPDX-License-Identifier: GPL-3.0-or-later
# Add the kernel module test

# syslogd daemon
add_executable(syslogd
    main.cpp
)

target_link_libraries(syslogd PRIVATE libsm::sm libipc)
target_include_directories(syslogd PRIVATE include)
add_to_initrd(TARGET syslogd /services)
add_to_initrd(FILE syslogd.toml /config/bits)

# syslogd client library
add_library(libsyslog SHARED libsyslogd.c)
target_include_directories(libsyslog PUBLIC include)
target_link_libraries(libsyslog PRIVATE mos::librpc-client)
add_to_initrd(TARGET libsyslog /lib)

# syslogd kernel module
mos_add_kernel_module(syslogd-kmod NAME syslogd SOURCES kmod.cpp)
target_include_directories(syslogd-kmod PRIVATE include)
add_to_initrd(MODULE syslogd-kmod /modules)
