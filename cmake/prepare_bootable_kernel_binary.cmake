# SPDX-License-Identifier: GPL-3.0-or-later

set(BOOT_DIR "${CMAKE_BINARY_DIR}/boot.dir")
set(MAPS_DIR "${CMAKE_BINARY_DIR}/boot.dir/maps")
set(KALLSYMS_DIR "${CMAKE_BINARY_DIR}/boot.dir/kallsyms")
make_directory(${BOOT_DIR})
make_directory(${MAPS_DIR})
make_directory(${KALLSYMS_DIR})

set(STUB_KALLSYMS_C [=[
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kallsyms.h"

const kallsyms_t mos_kallsyms[] = {
    { .address = 0, .name = "stub" },
}\;

]=])

file(WRITE ${CMAKE_BINARY_DIR}/stub_kallsyms.c ${STUB_KALLSYMS_C})

function(do_kallsyms TARGET_NAME LINKER_SCRIPT KALLSYMS_C LAUNCHER_ASM)
    add_executable(${TARGET_NAME} ${KALLSYMS_C} ${LAUNCHER_ASM})
    set_target_properties(${TARGET_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${BOOT_DIR}
        OUTPUT_NAME ${TARGET_NAME}
        EXCLUDE_FROM_ALL TRUE
        LINKER_LANGUAGE C
        LINK_DEPENDS "${LINKER_SCRIPT}"
        LINK_OPTIONS "-T${LINKER_SCRIPT};-nostdlib")
    target_link_libraries(${TARGET_NAME} PRIVATE gcc mos::kernel mos::private_include)
    add_custom_command(
        TARGET ${TARGET_NAME}
        POST_BUILD
        VERBATIM
        BYPRODUCTS "${MAPS_DIR}/${TARGET_NAME}.map"
        COMMAND nm -n $<TARGET_FILE:${TARGET_NAME}> > "${MAPS_DIR}/${TARGET_NAME}.map")
endfunction()

function(create_bootable_kernel_binary TARGET_NAME LINKER_SCRIPT OUTPUT_FILE LAUNCHER_ASM)
    set(STEP1_KALLSYMS_C "${CMAKE_BINARY_DIR}/stub_kallsyms.c")
    set(STEP2_KALLSYMS_C "${KALLSYMS_DIR}/${TARGET_NAME}.kallsyms.1.c")
    set(STEP3_KALLSYMS_C "${KALLSYMS_DIR}/${TARGET_NAME}.kallsyms.2.c")

    # Step 1: Compile the kernel with stub kallsyms
    do_kallsyms(${TARGET_NAME}.kallsyms.1 ${LINKER_SCRIPT} ${STEP1_KALLSYMS_C} ${LAUNCHER_ASM})

    # Step 2: Generate kallsyms.c from the map file for step 2 kallsyms
    add_custom_command(OUTPUT "${STEP2_KALLSYMS_C}"
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/gen_kallsyms.py ${MAPS_DIR}/${TARGET_NAME}.kallsyms.1.map "${STEP2_KALLSYMS_C}"
        DEPENDS ${MAPS_DIR}/${TARGET_NAME}.kallsyms.1.map
        VERBATIM)

    do_kallsyms(${TARGET_NAME}.kallsyms.2 ${LINKER_SCRIPT} ${STEP2_KALLSYMS_C} ${LAUNCHER_ASM})

    # Step 3: Generate kallsyms.c from the map file for step 3 kallsyms
    add_custom_command(OUTPUT "${STEP3_KALLSYMS_C}"
        COMMAND ${CMAKE_SOURCE_DIR}/scripts/gen_kallsyms.py ${MAPS_DIR}/${TARGET_NAME}.kallsyms.2.map "${STEP3_KALLSYMS_C}"
        DEPENDS ${MAPS_DIR}/${TARGET_NAME}.kallsyms.2.map ${LINKER_SCRIPT}
        VERBATIM)

    # Step 4: Compile the kernel with the final kallsyms
    do_kallsyms(${TARGET_NAME} ${LINKER_SCRIPT} ${STEP3_KALLSYMS_C} ${LAUNCHER_ASM})
    add_custom_command(
        TARGET ${TARGET_NAME}
        POST_BUILD
        VERBATIM
        BYPRODUCTS "${OUTPUT_FILE}"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${TARGET_NAME}> "${OUTPUT_FILE}"
        DEPENDS ${TARGET_NAME})
endfunction()
