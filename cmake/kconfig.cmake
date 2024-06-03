# SPDX-License-Identifier: Apache-2.0

if(NOT KCONFIG_ROOT_FILE)
    if (EXISTS ${CMAKE_SOURCE_DIR}/Kconfig)
        set(KCONFIG_ROOT_FILE ${CMAKE_SOURCE_DIR}/Kconfig)
    else()
        message(FATAL_ERROR "KCONFIG_ROOT_FILE not set")
    endif()
endif()

if (NOT EXISTS ${KCONFIG_ROOT_FILE})
    message(FATAL_ERROR "KCONFIG_ROOT_FILE does not exist: ${KCONFIG_ROOT_FILE}")
endif()

if(NOT DEFINED KCONFIG_PRESET_CONFIG)
    message(FATAL_ERROR "KCONFIG_PRESET_CONFIG not set")
endif()

if (NOT EXISTS ${KCONFIG_PRESET_CONFIG})
    message(FATAL_ERROR "KCONFIG_PRESET_CONFIG does not exist: ${KCONFIG_PRESET_CONFIG}")
endif()

if (NOT DEFINED KCONFIG_ARCH)
    message(FATAL_ERROR "KCONFIG_ARCH is not set")
endif()

if (NOT KCONFIG_PREFIX)
    set(KCONFIG_PREFIX "CONFIG_")
endif()

if (NOT DEFINED KCONFIG_ARCH_DIR)
    if (EXISTS ${CMAKE_SOURCE_DIR}/kernel/arch/${KCONFIG_ARCH})
        set(KCONFIG_ARCH_DIR kernel/arch/${KCONFIG_ARCH})
    else()
        message(FATAL_ERROR "KCONFIG_ARCH_DIR not set")
    endif()
endif()

#
# import_kconfig(<prefix> <kconfig_fragment> [<keys>])
#
# Parse a KConfig fragment (typically with extension .config) and
# introduce all the symbols that are prefixed with 'prefix' into the
# CMake namespace. List all created variable names in the 'keys'
# output variable if present.
function(import_kconfig prefix kconfig_fragment)
    # Parse the lines prefixed with 'prefix' in ${kconfig_fragment}
    file(
        STRINGS
        ${kconfig_fragment}
        DOT_CONFIG_LIST
        REGEX "^${prefix}"
        ENCODING "UTF-8"
    )

    foreach (CONFIG ${DOT_CONFIG_LIST})
        # CONFIG could look like: CONFIG_NET_BUF=y

        # Match the first part, the variable name
        string(REGEX MATCH "[^=]+" CONF_VARIABLE_NAME ${CONFIG})

        # Match the second part, variable value
        string(REGEX MATCH "=(.+$)" CONF_VARIABLE_VALUE ${CONFIG})
        # The variable name match we just did included the '=' symbol. To just get the
        # part on the RHS we use match group 1
        set(CONF_VARIABLE_VALUE ${CMAKE_MATCH_1})

        if("${CONF_VARIABLE_VALUE}" MATCHES "^\"(.*)\"$") # Is surrounded by quotes
            set(CONF_VARIABLE_VALUE ${CMAKE_MATCH_1})
        endif()

        set("${CONF_VARIABLE_NAME}" "${CONF_VARIABLE_VALUE}" PARENT_SCOPE)
        message(VERBOSE "Kconfig: Imported ${CONF_VARIABLE_NAME}=${CONF_VARIABLE_VALUE} from ${kconfig_fragment}")
        list(APPEND keys "${CONF_VARIABLE_NAME}")
    endforeach()

    foreach(outvar ${ARGN})
        set(${outvar} "${keys}" PARENT_SCOPE)
    endforeach()
endfunction()

find_program(PYTHON_EXECUTABLE NAMES python3 python REQUIRED)

# Folders needed for conf/mconf files (kconfig has no method of redirecting all output files).
# conf/mconf needs to be run from a different directory because of: GH-3408
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/kconfig/include/generated)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/kconfig/include/config)

set(KCONFIG_AUTOCONF_H ${CMAKE_BINARY_DIR}/kconfig/include/generated/autoconf.h)

set(DOTCONFIG                  ${CMAKE_BINARY_DIR}/.config)
set(PARSED_KCONFIG_SOURCES_TXT ${CMAKE_BINARY_DIR}/kconfig/sources.txt)

# Allow out-of-tree users to add their own Kconfig python frontend
# targets by appending targets to the CMake list
# 'EXTRA_KCONFIG_TARGETS' and setting variables named
# 'EXTRA_KCONFIG_TARGET_COMMAND_FOR_<target>'
#
# e.g.
# cmake -DEXTRA_KCONFIG_TARGETS=cli
# -DEXTRA_KCONFIG_TARGET_COMMAND_FOR_cli=cli_kconfig_frontend.py

set(EXTRA_KCONFIG_TARGET_COMMAND_FOR_menuconfig ${CMAKE_SOURCE_DIR}/scripts/kconfig/menuconfig.py)

foreach(kconfig_target menuconfig ${EXTRA_KCONFIG_TARGETS})
    add_custom_target(${kconfig_target}
        ${CMAKE_COMMAND} -E env
            srctree=${CMAKE_SOURCE_DIR}
            KCONFIG_CONFIG=${DOTCONFIG}
            ARCH=${KCONFIG_ARCH}
            ARCH_DIR=${KCONFIG_ARCH_DIR}
            CONFIG_=${KCONFIG_PREFIX}
            ${PYTHON_EXECUTABLE} ${EXTRA_KCONFIG_TARGET_COMMAND_FOR_${kconfig_target}} ${KCONFIG_ROOT_FILE}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/kconfig
        USES_TERMINAL
    )
endforeach()

# Support assigning Kconfig symbols on the command-line with CMake
# cache variables prefixed with 'CONFIG_'. This feature is
# experimental and undocumented until it has undergone more
# user-testing.
unset(EXTRA_KCONFIG_OPTIONS)
get_cmake_property(cache_variable_names CACHE_VARIABLES)
foreach(name ${cache_variable_names})
    if("${name}" MATCHES "^${KCONFIG_PREFIX}")
        set (_skip_ FALSE)
        foreach(_filter_ ${KCONFIG_EXTRA_OPTIONS_BLACKLIST})
            if("${name}" MATCHES "^${_filter_}$")
                set(_skip_ TRUE)
            endif()
        endforeach()

        if(_skip_)
            continue()
        endif()

        # if it's a string, quote it
        if (NOT "${${name}}" MATCHES "^([0-9]+|y|n)$")
            set(${name} "\"${${name}}\"")
        endif()

        # It is (then) assumed to be a Kconfig symbol assignment from the CMake command line.
        set(EXTRA_KCONFIG_OPTIONS "${EXTRA_KCONFIG_OPTIONS}\n${name}=${${name}}")
        message(VERBOSE "Kconfig: Assigned ${name}=${${name}} from CMake command line")
    endif()
endforeach()

if(EXTRA_KCONFIG_OPTIONS)
    set(EXTRA_KCONFIG_OPTIONS_FILE ${CMAKE_BINARY_DIR}/kconfig/cmake_cmdline.conf)
    file(WRITE ${EXTRA_KCONFIG_OPTIONS_FILE} ${EXTRA_KCONFIG_OPTIONS})
endif()

set(merge_config_files
    ${KCONFIG_PRESET_CONFIG}
    ${EXTRA_KCONFIG_OPTIONS_FILE}
)

# Create a list of absolute paths to the .config sources from
# merge_config_files, which is a mix of absolute and relative paths.
set(merge_config_files_with_absolute_paths "")
foreach(f ${merge_config_files})
    if(IS_ABSOLUTE ${f})
        set(path ${f})
    else()
        set(path ${CMAKE_SOURCE_DIR}/${f})
    endif()

    list(APPEND merge_config_files_with_absolute_paths ${path})
endforeach()

foreach(f ${merge_config_files_with_absolute_paths})
    if(NOT EXISTS ${f} OR IS_DIRECTORY ${f})
        message(FATAL_ERROR "File not found: ${f}")
    endif()
endforeach()

# Calculate a checksum of merge_config_files to determine if we need
# to re-generate .config
set(merge_config_files_checksum "")

foreach(f ${merge_config_files_with_absolute_paths})
    file(MD5 ${f} checksum)
    set(merge_config_files_checksum "${merge_config_files_checksum}${checksum}")
endforeach()

# Create a new .config if it does not exists, or if the checksum of
# the dependencies has changed
set(merge_config_files_checksum_file ${CMAKE_BINARY_DIR}/kconfig/merged.dotconfig.checksum)
set(CREATE_NEW_DOTCONFIG 1)

# Check if the checksum file exists too before trying to open it, though it
# should under normal circumstances
if(EXISTS ${DOTCONFIG} AND EXISTS ${merge_config_files_checksum_file})
    # Read out what the checksum was previously
    file(READ ${merge_config_files_checksum_file} merge_config_files_checksum_prev)
    if("${merge_config_files_checksum}" STREQUAL ${merge_config_files_checksum_prev})
        # Checksum is the same as before
        set(CREATE_NEW_DOTCONFIG 0)
    endif()
endif()

if(CREATE_NEW_DOTCONFIG)
    set(input_configs_are_handwritten --handwritten-input-configs)
    set(input_configs ${merge_config_files})
else()
    set(input_configs ${DOTCONFIG})
endif()

execute_process(
    COMMAND
    ${CMAKE_COMMAND} -E env
    srctree=${CMAKE_SOURCE_DIR}
    KCONFIG_CONFIG=${DOTCONFIG}
    ARCH=${KCONFIG_ARCH}
    ARCH_DIR=${KCONFIG_ARCH_DIR}
    CONFIG_=${KCONFIG_PREFIX}
    ${PYTHON_EXECUTABLE}
    ${CMAKE_SOURCE_DIR}/scripts/kconfig/kconfig.py
    ${input_configs_are_handwritten}
    ${KCONFIG_ROOT_FILE}
    ${DOTCONFIG}
    ${KCONFIG_AUTOCONF_H}
    ${PARSED_KCONFIG_SOURCES_TXT}
    ${input_configs}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    # The working directory is set to the app dir such that the user
    # can use relative paths in CONF_FILE, e.g. CONF_FILE=nrf5.conf
    RESULT_VARIABLE ret
)

if(NOT "${ret}" STREQUAL "0")
    message(FATAL_ERROR "command failed with return code: ${ret}")
endif()

if(CREATE_NEW_DOTCONFIG)
    # Write the new configuration fragment checksum. Only do this if kconfig.py
    # succeeds, to avoid marking zephyr/.config as up-to-date when it hasn't been
    # regenerated.
    file(WRITE ${merge_config_files_checksum_file} ${merge_config_files_checksum})
endif()

# Read out the list of 'Kconfig' sources that were used by the engine.
file(STRINGS ${PARSED_KCONFIG_SOURCES_TXT} PARSED_KCONFIG_SOURCES_LIST)

# Force CMAKE configure when the Kconfig sources or configuration files changes.
foreach(kconfig_input ${merge_config_files} ${DOTCONFIG} ${PARSED_KCONFIG_SOURCES_LIST} ${KCONFIG_AUTOCONF_H})
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${kconfig_input})
endforeach()

add_custom_target(config-sanitycheck DEPENDS ${DOTCONFIG})

# Remove the CLI Kconfig symbols from the namespace and
# CMakeCache.txt. If the symbols end up in DOTCONFIG they will be
# re-introduced to the namespace through 'import_kconfig'.
foreach (name ${cache_variable_names})
    if("${name}" MATCHES "^${KCONFIG_PREFIX}")
        if("${name}" IN_LIST KCONFIG_EXTRA_OPTIONS_BLACKLIST)
            continue()
        endif()
        unset(${name})
        unset(${name} CACHE)
        message(VERBOSE "Removed ${name} from cache")
    endif()
endforeach()

# Parse the lines prefixed with CONFIG_ in the .config file from Kconfig
import_kconfig(${KCONFIG_PREFIX} ${DOTCONFIG})

# Re-introduce the CLI Kconfig symbols that survived
foreach (name ${cache_variable_names})
    if("${name}" MATCHES "^${KCONFIG_PREFIX}")
        if("${name}" IN_LIST KCONFIG_EXTRA_OPTIONS_BLACKLIST)
            continue()
        endif()
        if(DEFINED ${name})
            set(${name} ${${name}} CACHE STRING "")
            message(VERBOSE "Re-introduced ${name} to cache")
        endif()
    endif()
endforeach()
