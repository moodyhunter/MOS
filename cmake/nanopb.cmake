# SPDX-License-Identifier: Zlib

set(NANOPB_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libs/nanopb/nanopb)
set(NANOPB_GENERATE_CPP_APPEND_PATH FALSE)

set(GENERATOR_PATH ${CMAKE_BINARY_DIR}/nanopb_workdir)
make_directory(${GENERATOR_PATH})

set(NANOPB_GENERATOR_EXECUTABLE ${GENERATOR_PATH}/nanopb_generator.py)
set(NANOPB_GENERATOR_PLUGIN ${GENERATOR_PATH}/protoc-gen-nanopb)
set(GENERATOR_CORE_DIR ${GENERATOR_PATH}/proto)
set(GENERATOR_CORE_SRC ${GENERATOR_CORE_DIR}/nanopb.proto)

# Treat the source directory as immutable.
#
# Copy the generator directory to the build directory before
# compiling python and proto files.  Fixes issues when using the
# same build directory with different python/protobuf versions
# as the binary build directory is discarded across builds.
#
# Notice: copy_directory does not copy the content if the directory already exists.
# We therefore append '/' to specify that we want to copy the content of the folder. See #847
#

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${NANOPB_SOURCE_DIR}/generator/ ${GENERATOR_PATH})

foreach(proto_file ${GENERATOR_CORE_SRC})
    get_filename_component(ABS_FILE ${proto_file} ABSOLUTE)
    execute_process(COMMAND protoc -I${GENERATOR_PATH}/proto --python_out=${GENERATOR_CORE_DIR} ${ABS_FILE})
endforeach()
