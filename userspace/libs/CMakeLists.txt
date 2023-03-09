# SPDX-License-Identifier: GPL-3.0-or-later

add_library(libuserspace OBJECT
    ${MOS_STANDARD_LIBRARY_SOURCES}
    libuserspace.c
    libuserspace++.cpp
    cxa.c
)

target_include_directories(libuserspace PUBLIC
    ${CMAKE_SOURCE_DIR}/userspace/include
    ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/include    # architecture-specific headers
)
target_link_libraries(libuserspace PUBLIC gcc mos::include)
add_library(mos::libuserspace ALIAS libuserspace)
add_dependencies(libuserspace mos_syscall_decl) # needs to be generated before we can compile

# TODO: Remove this once we have a proper userspace libc
target_compile_options(libuserspace PUBLIC "-ffreestanding")
target_link_options(libuserspace PUBLIC "-nostdlib")

add_subdirectory(libipc)
add_subdirectory(librpc)
