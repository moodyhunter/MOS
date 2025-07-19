# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(echo-ipc)
add_subdirectory(fork)
add_subdirectory(librpc)
add_subdirectory(ipc)
add_subdirectory(memfd)
add_subdirectory(libstdcxx)

add_subdirectory(libc-test)
add_simple_rust_project("${CMAKE_CURRENT_LIST_DIR}/rust-test" rust-test "/tests/")
add_subdirectory(signal)
add_subdirectory(pbtest)
add_subdirectory(pipe-test)

add_subdirectory(librpc-rs-test)
add_subdirectory(syslog-test)

add_subdirectory(vfs-test)

add_executable(test-launcher test-main.c)
add_to_initrd(TARGET test-launcher /tests)
