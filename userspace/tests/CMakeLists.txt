# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(echo-ipc)
add_subdirectory(fork)
add_subdirectory(librpc)
add_subdirectory(ipc)
add_subdirectory(cxx)
add_subdirectory(libstdcxx)

add_subdirectory(libc-test)
add_simple_rust_project("${CMAKE_CURRENT_LIST_DIR}/rust-test" rust-test "/tests/")
add_subdirectory(signal)
add_subdirectory(pipe-test)

add_subdirectory(librpc-rs-test)
add_subdirectory(syslog-test)

add_executable(test-launcher test-main.c)
setup_userspace_program(test-launcher /tests "Userspace test launcher")
