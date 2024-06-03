# SPDX-License-Identifier: Zlib
# Adapted from https://github.com/nanopb/nanopb/blob/master/extra/FindNanopb.cmake

set(nanopb_BUILD_RUNTIME OFF CACHE BOOL "don't build nanopb runtime" FORCE)
set(nanopb_BUILD_GENERATOR OFF CACHE BOOL "don't build nanopb generator" FORCE)

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

execute_process(
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${NANOPB_SOURCE_DIR}/generator/ ${GENERATOR_PATH}
)

foreach(_proto_file ${GENERATOR_CORE_SRC})
    get_filename_component(ABS_FILE ${_proto_file} ABSOLUTE)
    execute_process(COMMAND protoc -I${GENERATOR_PATH}/proto --python_out=${GENERATOR_CORE_DIR} ${ABS_FILE})
endforeach()

function(generate_nanopb_proto SRCS HDRS)
    cmake_parse_arguments(_arg "" "RELPATH" "" ${ARGN})

    if(NOT _arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "generate_nanopb_proto called without any proto files")
        return()
    endif()

    set(NANOPB_OPTIONS_DIRS)
    list(APPEND _nanopb_include_path "-I${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND _nanopb_include_path "-I${CMAKE_SOURCE_DIR}")

    if(DEFINED NANOPB_IMPORT_DIRS)
        foreach(DIR ${NANOPB_IMPORT_DIRS})
            get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
            list(APPEND _nanopb_include_path "-I${ABS_PATH}")
        endforeach()
    endif()

    list(REMOVE_DUPLICATES _nanopb_include_path)

    get_filename_component(ABS_ROOT ${CMAKE_SOURCE_DIR} ABSOLUTE)

    foreach(_proto_file ${_arg_UNPARSED_ARGUMENTS})
        get_filename_component(ABS_FILE ${_proto_file} ABSOLUTE)
        get_filename_component(FILE_WE ${_proto_file} NAME_WLE)
        get_filename_component(FILE_DIR ${ABS_FILE} PATH)

        cmake_path(RELATIVE_PATH ABS_FILE BASE_DIRECTORY ${ABS_ROOT} OUTPUT_VARIABLE FIL_REL)
        get_filename_component(FIL_PATH_REL ${FIL_REL} PATH)
        file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${FIL_PATH_REL})

        list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_PATH_REL}/${FILE_WE}.pb.c")
        list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_PATH_REL}/${FILE_WE}.pb.h")

        # If there an options file in the same working directory, set it as a dependency
        get_filename_component(ABS_OPT_FILE ${FILE_DIR}/${FILE_WE}.options ABSOLUTE)

        if(EXISTS ${ABS_OPT_FILE})
            # Get directory as lookups for dependency options fail if an options
            # file is used. The options is still set as a dependency of the
            # generated source and header.
            get_filename_component(options_dir ${ABS_OPT_FILE} DIRECTORY)
            list(APPEND NANOPB_OPTIONS_DIRS ${options_dir})
        else()
            set(ABS_OPT_FILE)
        endif()

        # If the dependencies are options files, we need to pass the directories
        # as arguments to nanopb
        foreach(dep ${NANOPB_DEPENDS})
            get_filename_component(ext ${dep} EXT)
            if(ext STREQUAL ".options")
                get_filename_component(depends_dir ${dep} DIRECTORY)
                list(APPEND NANOPB_OPTIONS_DIRS ${depends_dir})
            endif()
        endforeach()

        if(NANOPB_OPTIONS_DIRS)
            list(REMOVE_DUPLICATES NANOPB_OPTIONS_DIRS)
        endif()

        set(NANOPB_PLUGIN_OPTIONS)

        foreach(options_path ${NANOPB_OPTIONS_DIRS})
            set(NANOPB_PLUGIN_OPTIONS "${NANOPB_PLUGIN_OPTIONS} -I${options_path}")
        endforeach()

        if(NANOPB_OPTIONS)
            set(NANOPB_PLUGIN_OPTIONS "${NANOPB_PLUGIN_OPTIONS} ${NANOPB_OPTIONS}")
        endif()

        # based on the version of protoc it might be necessary to add "/${FIL_PATH_REL}" currently dealt with in #516
        set(NANOPB_OUT "${CMAKE_CURRENT_BINARY_DIR}")
        set(NANOPB_OPT_STRING "--nanopb_opt=${NANOPB_PLUGIN_OPTIONS}" "--nanopb_out=${NANOPB_OUT}")

        add_custom_command(
            OUTPUT
                "${CMAKE_CURRENT_BINARY_DIR}/${FIL_PATH_REL}/${FILE_WE}.pb.c"
                "${CMAKE_CURRENT_BINARY_DIR}/${FIL_PATH_REL}/${FILE_WE}.pb.h"
            COMMAND
                protoc
            ARGS
                ${_nanopb_include_path}
                -I${GENERATOR_PATH}
                -I${GENERATOR_CORE_DIR}
                -I${CMAKE_CURRENT_BINARY_DIR}
                -I${FILE_DIR}
                --plugin=protoc-gen-nanopb=${NANOPB_GENERATOR_PLUGIN}
                ${NANOPB_OPT_STRING}
                ${PROTOC_OPTIONS}
                ${ABS_FILE}
            DEPENDS
                ${ABS_FILE} ${ABS_OPT_FILE} ${NANOPB_DEPENDS}
            COMMENT "Running C++ protocol buffer compiler using nanopb plugin on ${_proto_file}"
            VERBATIM
        )
    endforeach()

    set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
    set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES INCLUDE_DIRECTORIES "${NANOPB_SOURCE_DIR};${CMAKE_CURRENT_BINARY_DIR}")

    set(${SRCS} ${${SRCS}} PARENT_SCOPE)
    set(${HDRS} ${${HDRS}} PARENT_SCOPE)
endfunction()

