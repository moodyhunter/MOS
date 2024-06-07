# SPDX-License-Identifier: GPL-3.0-or-later

include(cmake/userspace.cmake)

add_subdirectory(libs)
add_subdirectory(drivers)
add_subdirectory(programs)
add_subdirectory(tests)

# initrd resources
add_subdirectory(initrd)
