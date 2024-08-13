# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT NANOPB_SOURCE_DIR)
    message(FATAL_ERROR "NANOPB_SOURCE_DIR not set")
endif()

set(RPC_INCLUDE_DIR "${CMAKE_BINARY_DIR}/mosrpc")
set(RPC_OUTDIR "${CMAKE_BINARY_DIR}/mosrpc/proto")
set(MOSRPC_GENERATOR_PLUGIN "${CMAKE_SOURCE_DIR}/tools/protoc-gen-mosrpc/main.py")

get_filename_component(RPC_PROTO_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)     # absolute path to source directory

macro(generate_rpc_definitions SRCS HDRS)
    cmake_parse_arguments(ARG "" "" "PROTO_FILES" ${ARGN})

    if (ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "generate_rpc_definitions called with unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
        return()
    endif()

    if(NOT ARG_PROTO_FILES)
        message(FATAL_ERROR "generate_rpc_definitions called without any proto files")
        return()
    endif()

    set(NANOPB_OPTIONS_DIRS)

    set(${SRCS})
    set(${HDRS})

    foreach(proto_file ${ARG_PROTO_FILES})
        get_filename_component(PROTO_FILE_PATH_ABS ${proto_file} ABSOLUTE)  # absolute path to proto file
        get_filename_component(PROTO_FILE_NAME ${proto_file} NAME_WLE)      # filename without extension
        get_filename_component(PROTO_FILE_DIR ${PROTO_FILE_PATH_ABS} PATH)  # directory of proto file

        # PROTO_FILE_PATH_REL = relative path to proto file, relative to SOURCE_DIR_ABS
        cmake_path(RELATIVE_PATH PROTO_FILE_PATH_ABS BASE_DIRECTORY ${RPC_PROTO_SOURCE_DIR} OUTPUT_VARIABLE PROTO_FILE_PATH_REL)
        get_filename_component(PROTO_FILE_DIR_REL ${PROTO_FILE_PATH_REL} PATH)  # directory of proto file, relative to SOURCE_DIR_ABS

        file(MAKE_DIRECTORY ${RPC_OUTDIR}/${PROTO_FILE_DIR_REL})

        set(GENERATED_SRC_FILE "${RPC_OUTDIR}/${PROTO_FILE_DIR_REL}/${PROTO_FILE_NAME}.pb.c")
        set(GENERATED_HDR_FILE "${RPC_OUTDIR}/${PROTO_FILE_DIR_REL}/${PROTO_FILE_NAME}.pb.h")
        set(GENERATED_SERVICE_FILE)

        # check if 'service ' is present in the proto file
        file(STRINGS ${PROTO_FILE_PATH_ABS} PROTO_FILE_CONTENTS)
        foreach(line ${PROTO_FILE_CONTENTS})
            if(line MATCHES "^service [A-Za-z0-9_]+")
                message(STATUS "Found service definition '${line}' in ${proto_file}")
                set(GENERATED_SERVICE_FILE "${RPC_OUTDIR}/${PROTO_FILE_DIR_REL}/${PROTO_FILE_NAME}.services.h")
                list(APPEND ${HDRS} "${GENERATED_SERVICE_FILE}")
                break() # only need to find one service definition
            endif()
        endforeach()

        # If there an options file in the same working directory, set it as a dependency
        get_filename_component(ABS_OPT_FILE ${PROTO_FILE_DIR}/${PROTO_FILE_NAME}.options ABSOLUTE)

        if(EXISTS ${ABS_OPT_FILE})
            # Get directory as lookups for dependency options fail if an options
            # file is used. The options is still set as a dependency of the
            # generated source and header.
            get_filename_component(options_dir ${ABS_OPT_FILE} DIRECTORY)
            list(APPEND NANOPB_OPTIONS_DIRS ${options_dir})
        else()
            set(ABS_OPT_FILE)
        endif()

        if(NANOPB_OPTIONS_DIRS)
            list(REMOVE_DUPLICATES NANOPB_OPTIONS_DIRS)
        endif()

        set(NANOPB_PLUGIN_OPTIONS)

        foreach(options_path ${NANOPB_OPTIONS_DIRS})
            set(NANOPB_PLUGIN_OPTIONS "${NANOPB_PLUGIN_OPTIONS} -I${options_path}")
        endforeach()

        add_custom_command(
            OUTPUT
                ${GENERATED_SRC_FILE}
                ${GENERATED_HDR_FILE}
                ${GENERATED_SERVICE_FILE}
            COMMAND
                protoc
            ARGS
                -I${CMAKE_CURRENT_SOURCE_DIR}
                -I${CMAKE_SOURCE_DIR}
                -I${GENERATOR_CORE_DIR}
                -I${PROTO_FILE_DIR}

                # NanoPB Generator
                --plugin=protoc-gen-nanopb=${NANOPB_GENERATOR_PLUGIN}
                --nanopb_opt=${NANOPB_PLUGIN_OPTIONS}
                --nanopb_out=${RPC_OUTDIR}

                # MOS RPC Service Generator
                --plugin=protoc-gen-mosrpc=${MOSRPC_GENERATOR_PLUGIN}
                --mosrpc_out=${RPC_OUTDIR}
                ${PROTO_FILE_PATH_ABS}
            DEPENDS
                ${PROTO_FILE_PATH_ABS} ${ABS_OPT_FILE} ${NANOPB_GENERATOR_PLUGIN} ${MOSRPC_GENERATOR_PLUGIN}
            COMMENT "Generating RPC protocol for ${PROTO_FILE_NAME}.proto"
            VERBATIM
        )

        set_source_files_properties(${GENERATED_SRC_FILE} PROPERTIES GENERATED TRUE)
        set_source_files_properties(${GENERATED_HDR_FILE} PROPERTIES GENERATED TRUE)
        set_source_files_properties(${GENERATED_SERVICE_FILE} PROPERTIES GENERATED TRUE)

        list(APPEND ${SRCS} ${GENERATED_SRC_FILE})
        list(APPEND ${HDRS} ${GENERATED_HDR_FILE} ${GENERATED_SERVICE_FILE})
    endforeach()
endmacro()

file(GLOB PROTO_FILES "*.proto")

generate_rpc_definitions(
    RPC_SRCS
    RPC_HDRS
    PROTO_FILES ${PROTO_FILES}
)

add_custom_target(mos_generate_rpc_protocols
    SOURCES ${PROTO_FILES}
    VERBATIM
    COMMENT "Generating MOS RPC protocols"
    DEPENDS ${RPC_SRCS} ${RPC_HDRS}
)

add_library(mos_rpc_protocols_src INTERFACE)
target_sources(mos_rpc_protocols_src INTERFACE ${RPC_SRCS} ${RPC_HDRS})
add_dependencies(mos_rpc_protocols_src mos_generate_rpc_protocols)
target_include_directories(mos_rpc_protocols_src INTERFACE ${RPC_INCLUDE_DIR})

add_library(mos::rpc-protocols ALIAS mos_rpc_protocols_src)
