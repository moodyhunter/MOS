# SPDX-License-Identifier: GPL-3.0-or-later

include(cmake/userspace.cmake)

add_subdirectory(libs)
add_subdirectory(drivers)
add_subdirectory(programs)
add_subdirectory(services)
add_subdirectory(tests)

add_subdirectory(games)

# initrd resources
add_subdirectory(initrd)

add_library(mosrpc SHARED)
target_link_libraries(mosrpc PRIVATE mos::rpc-protocols)
target_link_libraries(mosrpc PUBLIC mos::librpc-client mos::librpc-server)
target_include_directories(mosrpc PUBLIC $<INSTALL_INTERFACE:include/mosrpc> $<INSTALL_INTERFACE:include/mosrpc/proto>)
install(TARGETS mosrpc
    EXPORT MOSSDK
    COMPONENT sdk
    EXCLUDE_FROM_ALL
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

add_to_initrd(TARGET mosrpc "/lib/")
