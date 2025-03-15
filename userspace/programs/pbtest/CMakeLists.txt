# SPDX-License-Identifier: GPL-3.0-or-later

if (__MOS_HAS_NO_COMPILER)
    return()
endif()

find_package(Protobuf)

if (NOT Protobuf_FOUND)
    message(WARNING "Protobuf not found, not building protobuf_test")
    return()
endif()

# Find the protobuf compiler
set(PROTOC "/usr/bin/protoc")

add_executable(pbtest main.cpp)
target_link_options(pbtest PRIVATE "-Wl,--copy-dt-needed-entries")
target_link_libraries(pbtest protobuf::libprotobuf)

# Generate the protobuf files
add_custom_command(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/test.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/test.pb.h
    COMMAND ${PROTOC}
    ARGS
    -I ${CMAKE_CURRENT_SOURCE_DIR}
     --cpp_out ${CMAKE_CURRENT_BINARY_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/test.proto
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test.proto
    COMMENT "Generating test.pb.cc and test.pb.h"
    VERBATIM
)

target_include_directories(pbtest PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Add the generated files to the target
target_sources(pbtest PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/test.pb.cc
    ${CMAKE_CURRENT_BINARY_DIR}/test.pb.h)

add_to_initrd(TARGET pbtest /tests)
