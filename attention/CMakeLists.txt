set(TPA_ATTENTION_PLANNER_ROOT "${TPA_ROOT_DIR}/planner")
set(TPA_ATTENTION_PLANNER_PYTHONPATH "${TPA_ATTENTION_PLANNER_ROOT}/src")
if (DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
    set(TPA_ATTENTION_PLANNER_PYTHONPATH
        "${TPA_ATTENTION_PLANNER_PYTHONPATH}:$ENV{PYTHONPATH}")
endif()
set(TPA_ATTENTION_PLANNER_EXTRACT
    "${TPA_ATTENTION_PLANNER_ROOT}/src/tpa_planner/extract_process_metadata.py")
set(TPA_ATTENTION_PLANNER_MAP
    "${TPA_ATTENTION_PLANNER_ROOT}/src/tpa_planner/map_program.py")
set(TPA_ATTENTION_PLANNER_COMMON
    "${TPA_ATTENTION_PLANNER_ROOT}/src/tpa_planner/common.py")
set(TPA_ATTENTION_PLANNER_EDGE
    "${TPA_ATTENTION_PLANNER_ROOT}/src/tpa_planner/edge_plan.py")
set(TPA_ATTENTION_MACHINE_JSON "${TPA_ROOT_DIR}/machines/erbium.json")
set(TPA_ATTENTION_COMPUTE_COSTS_JSON
    "${CMAKE_CURRENT_SOURCE_DIR}/attention_compute_costs.json")
set(TPA_ATTENTION_METADATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/metadata")
set(TPA_ATTENTION_REPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/planner")

set(PYTHON "" CACHE FILEPATH "Python interpreter")
if (NOT PYTHON)
    set(Python3_FIND_VIRTUALENV FIRST)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(PYTHON "${Python3_EXECUTABLE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PYTHONPATH=${TPA_ATTENTION_PLANNER_PYTHONPATH}"
        "${PYTHON}" -c
        "import tpa_planner.extract_process_metadata; import tpa_planner.map_program"
    RESULT_VARIABLE TPA_ATTENTION_PLANNER_IMPORT_RESULT
    ERROR_VARIABLE TPA_ATTENTION_PLANNER_IMPORT_ERROR
    ERROR_STRIP_TRAILING_WHITESPACE
)
if (TPA_ATTENTION_PLANNER_IMPORT_RESULT)
    message(FATAL_ERROR
        "TPA planner package is not available for ${PYTHON}.\n"
        "Install it with:\n"
        "  ${PYTHON} -m pip install -e ${TPA_ATTENTION_PLANNER_ROOT}\n"
        "or reconfigure with:\n"
        "  -DPYTHON=/path/to/python\n"
        "Import error:\n"
        "${TPA_ATTENTION_PLANNER_IMPORT_ERROR}"
    )
endif()

function(attention_add_process_metadata TARGET_NAME MANIFEST_PATH)
    set(JSON_OUT "${TPA_ATTENTION_METADATA_DIR}/${TARGET_NAME}.json")

    add_custom_command(
        OUTPUT "${JSON_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${TPA_ATTENTION_METADATA_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "PYTHONPATH=${TPA_ATTENTION_PLANNER_PYTHONPATH}"
            "${PYTHON}" -m tpa_planner.extract_process_metadata
            --label "${TARGET_NAME}"
            --manifest "${MANIFEST_PATH}"
            --output "${JSON_OUT}"
            --objects $<TARGET_OBJECTS:${TARGET_NAME}>
        DEPENDS
            ${TARGET_NAME}
            "${MANIFEST_PATH}"
            "${TPA_ATTENTION_PLANNER_EXTRACT}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(${TARGET_NAME}_metadata_json DEPENDS "${JSON_OUT}")
    add_dependencies(${TARGET_NAME}_metadata_json ${TARGET_NAME})
    set_target_properties(${TARGET_NAME} PROPERTIES TPA_METADATA_JSON "${JSON_OUT}")
endfunction()

function(attention_add_mapped_program NAME PROGRAM_PATH)
    set(options)
    set(oneValueArgs OUT_PLACEMENT_VAR OUT_EDGE_HEADER_VAR OUT_REPORT_VAR OUT_MAPPED_PROGRAM_VAR)
    set(multiValueArgs PROCESS_TARGETS)
    cmake_parse_arguments(ATTNMAP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(PROCESS_JSONS)
    set(PROCESS_JSON_TARGETS)
    foreach (PROC_TARGET IN LISTS ATTNMAP_PROCESS_TARGETS)
        get_target_property(PROC_JSON ${PROC_TARGET} TPA_METADATA_JSON)
        if (NOT PROC_JSON)
            message(FATAL_ERROR "No TPA_METADATA_JSON property for ${PROC_TARGET}")
        endif()
        list(APPEND PROCESS_JSONS "${PROC_JSON}")
        list(APPEND PROCESS_JSON_TARGETS ${PROC_TARGET}_metadata_json)
    endforeach()

    set(REPORT_OUT "${TPA_ATTENTION_REPORT_DIR}/${NAME}_map.json")
    set(MAPPED_PROGRAM_OUT
        "${TPA_ATTENTION_REPORT_DIR}/${NAME}_mapped_program.json")
    set(PLACEMENT_OUT "${TPA_ATTENTION_REPORT_DIR}/${NAME}.place")
    set(SCRATCH_HEADER_OUT
        "${TPA_ATTENTION_REPORT_DIR}/${NAME}_scratch_config.h")
    set(EDGE_HEADER_OUT "${TPA_ATTENTION_REPORT_DIR}/${NAME}_edge_config.h")

    add_custom_command(
        OUTPUT
            "${REPORT_OUT}"
            "${MAPPED_PROGRAM_OUT}"
            "${PLACEMENT_OUT}"
            "${SCRATCH_HEADER_OUT}"
            "${EDGE_HEADER_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${TPA_ATTENTION_REPORT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "PYTHONPATH=${TPA_ATTENTION_PLANNER_PYTHONPATH}"
            "${PYTHON}" -m tpa_planner.map_program
            --program "${PROGRAM_PATH}"
            --process-jsons ${PROCESS_JSONS}
            --compute-costs-json "${TPA_ATTENTION_COMPUTE_COSTS_JSON}"
            --machine-json "${TPA_ATTENTION_MACHINE_JSON}"
            --output "${REPORT_OUT}"
            --mapped-program-out "${MAPPED_PROGRAM_OUT}"
            --placement-out "${PLACEMENT_OUT}"
            --scratch-header-out "${SCRATCH_HEADER_OUT}"
            --edge-config-header-out "${EDGE_HEADER_OUT}"
        DEPENDS
            ${PROCESS_JSON_TARGETS}
            "${PROGRAM_PATH}"
            "${TPA_ATTENTION_COMPUTE_COSTS_JSON}"
            "${TPA_ATTENTION_MACHINE_JSON}"
            "${TPA_ATTENTION_PLANNER_MAP}"
            "${TPA_ATTENTION_PLANNER_COMMON}"
            "${TPA_ATTENTION_PLANNER_EDGE}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(${NAME}_mapped_program DEPENDS
        "${REPORT_OUT}"
        "${MAPPED_PROGRAM_OUT}"
        "${PLACEMENT_OUT}"
        "${SCRATCH_HEADER_OUT}"
        "${EDGE_HEADER_OUT}"
    )

    if (ATTNMAP_OUT_PLACEMENT_VAR)
        set(${ATTNMAP_OUT_PLACEMENT_VAR} "${PLACEMENT_OUT}" PARENT_SCOPE)
    endif()
    if (ATTNMAP_OUT_EDGE_HEADER_VAR)
        set(${ATTNMAP_OUT_EDGE_HEADER_VAR} "${EDGE_HEADER_OUT}" PARENT_SCOPE)
    endif()
    if (ATTNMAP_OUT_REPORT_VAR)
        set(${ATTNMAP_OUT_REPORT_VAR} "${REPORT_OUT}" PARENT_SCOPE)
    endif()
    if (ATTNMAP_OUT_MAPPED_PROGRAM_VAR)
        set(${ATTNMAP_OUT_MAPPED_PROGRAM_VAR} "${MAPPED_PROGRAM_OUT}" PARENT_SCOPE)
    endif()
endfunction()

add_tpa_process(
    NAME tpa_attention_qkv_gen_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_qkv_gen.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_qkv_gen.c"
)
attention_add_process_metadata(
    tpa_attention_qkv_gen_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/attention_qkv_gen.tpm"
)

add_tpa_process(
    NAME tpa_attention_score_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_score.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_score.c"
)
attention_add_process_metadata(
    tpa_attention_score_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/attention_score.tpm"
)

add_tpa_process(
    NAME tpa_attention_softmax_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_softmax.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_softmax.c"
)
attention_add_process_metadata(
    tpa_attention_softmax_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/attention_softmax.tpm"
)

add_tpa_process(
    NAME tpa_attention_output_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_output.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_output.c"
)
attention_add_process_metadata(
    tpa_attention_output_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/attention_output.tpm"
)

set(tpa_attention_processes
    tpa_attention_qkv_gen_proc
    tpa_attention_score_proc
    tpa_attention_softmax_proc
    tpa_attention_output_proc
)

attention_add_mapped_program(
    tpa_fast_attention_map
    "${CMAKE_CURRENT_SOURCE_DIR}/attention.tpp"
    PROCESS_TARGETS
        ${tpa_attention_processes}
    OUT_PLACEMENT_VAR TPA_FAST_ATTENTION_MAPPED_PLACEMENT
    OUT_EDGE_HEADER_VAR TPA_FAST_ATTENTION_EDGE_HEADER
    OUT_REPORT_VAR TPA_FAST_ATTENTION_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_FAST_ATTENTION_MAPPED_PROGRAM
)

add_tpa_program(
    NAME tpa_fast_attention
    PROCESSES
        ${tpa_attention_processes}
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/attention.tpp"
    PLACEMENT
        "${TPA_FAST_ATTENTION_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER
        "${TPA_FAST_ATTENTION_EDGE_HEADER}"
)
add_dependencies(tpa_fast_attention.elf tpa_fast_attention_map_mapped_program)

add_tpa_program(
    NAME tpa_fast_attention_serial
    PROCESSES
        ${tpa_attention_processes}
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/attention.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_serial.place"
)
