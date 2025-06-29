# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(syslog-test main.cpp)
target_link_libraries(syslog-test PRIVATE libsyslog)
add_to_initrd(TARGET syslog-test /tests)
