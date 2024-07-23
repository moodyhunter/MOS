# SPDX-License-Identifier: GPL-3.0-or-later

set(BOOT_DIR "${CMAKE_BINARY_DIR}/boot.dir")
set(MAPS_DIR "${CMAKE_BINARY_DIR}/boot.dir/maps")
set(KALLSYMS_DIR "${CMAKE_BINARY_DIR}/boot.dir/kallsyms")
make_directory(${BOOT_DIR})
make_directory(${MAPS_DIR})
make_directory(${KALLSYMS_DIR})

set(STUB_KALLSYMS_C [=[
// SPDX-License-Identifier: GPL-3.0-or-later
// !! This file is generated by CMake, any changes will be overwritten

#include "mos/misc/kallsyms.h"

const kallsyms_t mos_kallsyms[] = {
    { .address = 0, .name = "stub" },
}\;
]=])

file(WRITE ${KALLSYMS_DIR}/stub_kallsyms.c ${STUB_KALLSYMS_C})

function(do_kallsyms TARGET_NAME LINKER_SCRIPT KALLSYMS_C)
    add_executable(${TARGET_NAME} ${KALLSYMS_C})
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BOOT_DIR}/kallsyms/
        OUTPUT_NAME ${TARGET_NAME}
        EXCLUDE_FROM_ALL TRUE
        LINKER_LANGUAGE C
        LINK_DEPENDS "${LINKER_SCRIPT}"
        LINK_OPTIONS "-T${LINKER_SCRIPT};-nostdlib;-Wl,--build-id=none")

    # link mos::kernel explicitly to ensure that all symbols from the kernel are
    # included in the final binary (linker may strip unused symbols in ${TARGET_NAME}_loader)
    target_link_libraries(${TARGET_NAME} PRIVATE ${ARGS_TARGET}_loader mos::kernel)
    add_custom_command(
        TARGET ${TARGET_NAME}
        POST_BUILD
        VERBATIM
        BYPRODUCTS "${MAPS_DIR}/${TARGET_NAME}.map"
        COMMAND nm -C -n $<TARGET_FILE:${TARGET_NAME}> > "${MAPS_DIR}/${TARGET_NAME}.map")
endfunction()

### Create a bootable kernel binary: (TARGET <TARGET_NAME> LINKER_SCRIPT <LINKER_SCRIPT> FILENAME <OUTPUT_FILE> SOURCES [ADDITIONAL_SOURCES...])
function(create_bootable_kernel_binary)
    cmake_parse_arguments(ARGS "" "TARGET;COMMENT;LINKER_SCRIPT;FILENAME" "SOURCES" ${ARGN})

    if (ARGS_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown arguments to create_bootable_kernel_binary: ${ARGS_UNPARSED_ARGUMENTS}")
    endif()

    if (NOT IS_ABSOLUTE ${ARGS_LINKER_SCRIPT})
        set(ARGS_LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/${ARGS_LINKER_SCRIPT}")
    endif()

    if (NOT ARGS_TARGET)
        message(FATAL_ERROR "create_bootable_kernel_binary: TARGET argument is required")
    endif()

    if (NOT ARGS_FILENAME)
        message(FATAL_ERROR "create_bootable_kernel_binary: FILENAME argument is required")
    endif()

    if (NOT ARGS_COMMENT)
        set(ARGS_COMMENT "Bootable kernel ${ARGS_FILENAME}")
    endif()

    # add loader code to the kernel
    add_library(${ARGS_TARGET}_loader OBJECT ${ARGS_SOURCES})
    # no need to publicly link to mos::kernel, see above comment in do_kallsyms
    target_link_libraries(${ARGS_TARGET}_loader PRIVATE mos::kernel)

    set(STEP1_KALLSYMS_C "${KALLSYMS_DIR}/stub_kallsyms.c")
    set(STEP2_KALLSYMS_C "${KALLSYMS_DIR}/${ARGS_TARGET}.kallsyms.1.c")
    set(STEP3_KALLSYMS_C "${KALLSYMS_DIR}/${ARGS_TARGET}.kallsyms.2.c")

    # Step 1: Compile the kernel with stub kallsyms
    do_kallsyms(${ARGS_TARGET}.kallsyms.1 ${ARGS_LINKER_SCRIPT} ${STEP1_KALLSYMS_C})

    # Step 2: Generate kallsyms.c from the map file for step 2 kallsyms
    add_custom_command(OUTPUT "${STEP2_KALLSYMS_C}"
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/gen_kallsyms.py ${MAPS_DIR}/${ARGS_TARGET}.kallsyms.1.map "${STEP2_KALLSYMS_C}"
        DEPENDS ${MAPS_DIR}/${ARGS_TARGET}.kallsyms.1.map
        VERBATIM)

    do_kallsyms(${ARGS_TARGET}.kallsyms.2 ${ARGS_LINKER_SCRIPT} ${STEP2_KALLSYMS_C})

    # Step 3: Generate kallsyms.c from the map file for step 3 kallsyms
    add_custom_command(OUTPUT "${STEP3_KALLSYMS_C}"
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/gen_kallsyms.py ${MAPS_DIR}/${ARGS_TARGET}.kallsyms.2.map "${STEP3_KALLSYMS_C}"
        DEPENDS ${MAPS_DIR}/${ARGS_TARGET}.kallsyms.2.map ${ARGS_LINKER_SCRIPT}
        VERBATIM)

    # Step 4: Compile the kernel with the final kallsyms
    add_executable(${ARGS_TARGET} ${STEP3_KALLSYMS_C})
    set_target_properties(${ARGS_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BOOT_DIR}/
        OUTPUT_NAME ${ARGS_FILENAME}
        EXCLUDE_FROM_ALL TRUE
        LINKER_LANGUAGE C
        LINK_DEPENDS "${ARGS_LINKER_SCRIPT}"
        LINK_OPTIONS "-T${ARGS_LINKER_SCRIPT};-nostdlib;-Wl,--build-id=none")

    # link mos::kernel explicitly to ensure that all symbols from the kernel are
    # included in the final binary (linker may strip unused symbols in ${TARGET_NAME}_loader)
    target_link_libraries(${ARGS_TARGET} PRIVATE ${ARGS_TARGET}_loader mos::kernel)
    add_summary_item(BOOTABLE ${ARGS_TARGET} "${ARGS_COMMENT}")
endfunction()
