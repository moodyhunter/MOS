# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(syslogd
    main.c
)

target_include_directories(syslogd
    PRIVATE
        include
)

target_link_libraries(syslogd
    PRIVATE
        mos::librpc-server_hosted
)

add_to_initrd(TARGET syslogd /programs)

add_library(libsyslog INTERFACE)
target_include_directories(libsyslog INTERFACE include)
target_link_libraries(libsyslog INTERFACE mos::librpc-client_hosted)
