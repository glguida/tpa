include(CMakeParseArguments)

if (NOT DEFINED TPA_ROOT_DIR)
    set(TPA_ROOT_DIR "${PROJECT_SOURCE_DIR}")
endif()

set(TPA_PLATFORM "erbium" CACHE STRING "TPA device platform")
set_property(CACHE TPA_PLATFORM PROPERTY STRINGS erbium etsoc1)
string(TOLOWER "${TPA_PLATFORM}" TPA_PLATFORM_ID)

if (TPA_PLATFORM_ID STREQUAL "erbium")
    set(TPA_PLATFORM_LINKER_SCRIPT "${TPA_ROOT_DIR}/platform/erbium_mram.ld")
    set(TPA_PLATFORM_SHARED_INC_DIR "${TPA_ROOT_DIR}/platform")
    set(TPA_PLATFORM_SOURCES
        "${TPA_ROOT_DIR}/platform/boot.S"
        "${TPA_ROOT_DIR}/platform/crt.S"
        "${TPA_ROOT_DIR}/platform/trap.S"
    )
else()
    set(TPA_PLATFORM_LINKER_SCRIPT "")
    set(TPA_PLATFORM_SHARED_INC_DIR "")
    set(TPA_PLATFORM_SOURCES)
endif()

function(add_tpa_process)
    set(options)
    set(oneValueArgs NAME MANIFEST)
    set(multiValueArgs SOURCES INCLUDES LIBS)
    cmake_parse_arguments(TPAPROC "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT TPAPROC_NAME)
        message(FATAL_ERROR "add_tpa_process() needs NAME")
    endif()
    if (NOT TPAPROC_MANIFEST)
        message(FATAL_ERROR "add_tpa_process() needs MANIFEST")
    endif()
    if (NOT TPAPROC_SOURCES)
        message(FATAL_ERROR "add_tpa_process() needs SOURCES")
    endif()

    add_library(${TPAPROC_NAME} OBJECT ${TPAPROC_SOURCES})
    target_link_libraries(${TPAPROC_NAME} PRIVATE tpa_core et-common-libs::cm-umode ${TPAPROC_LIBS})
    target_include_directories(${TPAPROC_NAME} PRIVATE
        "${TPA_ROOT_DIR}/include"
        ${TPAPROC_INCLUDES}
    )
    target_compile_features(${TPAPROC_NAME} PRIVATE c_std_11)
    target_compile_options(${TPAPROC_NAME} PRIVATE -Wall -Wextra -Werror -Wno-cast-qual -Wno-unused-function -Wno-unused-const-variable)
    set_target_properties(${TPAPROC_NAME} PROPERTIES TPA_MANIFEST "${TPAPROC_MANIFEST}")
endfunction()

function(add_tpa_program_test)
    add_tpa_program(${ARGN})
endfunction()

function(add_tpa_program)
    set(options)
    set(oneValueArgs NAME PROGRAM PLACEMENT LINKER_SCRIPT OUTPUT_DIR EDGE_CONFIG_HEADER)
    set(multiValueArgs PROCESSES INCLUDES LIBS)
    cmake_parse_arguments(TPAPROG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT TPAPROG_NAME)
        message(FATAL_ERROR "add_tpa_program() needs NAME")
    endif()
    if (NOT TPAPROG_PROGRAM)
        message(FATAL_ERROR "add_tpa_program() needs PROGRAM")
    endif()
    if (NOT TPAPROG_PLACEMENT)
        message(FATAL_ERROR "add_tpa_program() needs PLACEMENT")
    endif()
    if (NOT TPAPROG_PROCESSES)
        message(FATAL_ERROR "add_tpa_program() needs PROCESSES")
    endif()

    set(proc_objs)
    set(proc_manifests)
    foreach (proc IN LISTS TPAPROG_PROCESSES)
        if (NOT TARGET ${proc})
            message(FATAL_ERROR "add_tpa_program(): '${proc}' is not a target")
        endif()
        get_target_property(proc_manifest ${proc} TPA_MANIFEST)
        if (NOT proc_manifest)
            message(FATAL_ERROR "add_tpa_program(): '${proc}' has no TPA_MANIFEST")
        endif()
        list(APPEND proc_objs "$<TARGET_OBJECTS:${proc}>")
        list(APPEND proc_manifests "${proc_manifest}")
    endforeach()

    list(JOIN proc_manifests "|" proc_manifests_arg)
    set(tpa_img_src "${CMAKE_CURRENT_BINARY_DIR}/generated/${TPAPROG_NAME}_image.c")
    set(tpa_img_deps
        ${proc_manifests}
        "${TPAPROG_PROGRAM}"
        "${TPAPROG_PLACEMENT}"
        "${TPA_ROOT_DIR}/cmake/gen_tpa_image.cmake"
    )
    if (TPAPROG_EDGE_CONFIG_HEADER)
        list(APPEND tpa_img_deps "${TPAPROG_EDGE_CONFIG_HEADER}")
    endif()

    add_custom_command(
        OUTPUT "${tpa_img_src}"
        COMMAND "${CMAKE_COMMAND}"
            -DPROCESS_MANIFESTS=${proc_manifests_arg}
            -DPROGRAM=${TPAPROG_PROGRAM}
            -DPLACEMENT=${TPAPROG_PLACEMENT}
            -DOUTPUT=${tpa_img_src}
            -DEDGE_CONFIG_HEADER=${TPAPROG_EDGE_CONFIG_HEADER}
            -P "${TPA_ROOT_DIR}/cmake/gen_tpa_image.cmake"
        DEPENDS ${tpa_img_deps}
        VERBATIM
    )

    if (TPA_PLATFORM_LINKER_SCRIPT)
        set(LINKER_SCRIPT "${TPA_PLATFORM_LINKER_SCRIPT}")
        set(SHARED_INC_DIR "${TPA_PLATFORM_SHARED_INC_DIR}")
    endif()

    add_riscv_executable(${TPAPROG_NAME}
        ${TPA_PLATFORM_SOURCES}
        "${TPA_ROOT_DIR}/tpa-device/runtime/demo_runtime.c"
        "${tpa_img_src}"
        ${proc_objs}
    )

    unset(LINKER_SCRIPT)
    unset(SHARED_INC_DIR)

    target_link_libraries(${TPAPROG_NAME}.elf PRIVATE tpa_core et-common-libs::cm-umode ${TPAPROG_LIBS})
    target_link_options(${TPAPROG_NAME}.elf PRIVATE -Wl,--no-warn-rwx-segments)
    target_include_directories(${TPAPROG_NAME}.elf PRIVATE
        "${TPA_ROOT_DIR}/include"
        ${TPAPROG_INCLUDES}
    )
    target_compile_features(${TPAPROG_NAME}.elf PRIVATE c_std_11)
    target_compile_options(${TPAPROG_NAME}.elf PRIVATE -Wall -Wextra -Werror -Wno-cast-qual)
    set_target_properties(${TPAPROG_NAME}.elf PROPERTIES LINKER_LANGUAGE C)
    add_dependencies(${TPAPROG_NAME}.elf ${TPAPROG_PROCESSES})
endfunction()
