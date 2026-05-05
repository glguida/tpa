set(TPA_YOLOV8N_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")
set(TPA_YOLOV8N_EXPECT_HEADER_SHA256
    "54f3972ca72d4493b0fc395acb82669427f1068bbdbfe7157de872fcacfa9563")
set(TPA_YOLOV8N_EXPECT_MANIFEST_SHA256
    "199ad6ebd7f499030baf4325db33181d51414ef1ff20960577405ff95798d19a")

set(TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER "" CACHE FILEPATH
    "External generated YOLOv8n weight header; model-derived and not checked in")
set(TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST "" CACHE FILEPATH
    "External generated YOLOv8n weight manifest; model-derived and not checked in")

if (NOT TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER)
    message(FATAL_ERROR
        "BUILD_TPA_YOLOV8N=ON requires -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=/path/to/yolov8n_external_detect_c2f_weights.h")
endif()
if (NOT EXISTS "${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}")
    message(FATAL_ERROR
        "TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER does not exist: ${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}")
endif()
if (NOT TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST)
    message(FATAL_ERROR
        "BUILD_TPA_YOLOV8N=ON requires -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=/path/to/yolov8n_external_detect_c2f_generated_manifest.json")
endif()
if (NOT EXISTS "${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}")
    message(FATAL_ERROR
        "TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST does not exist: ${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}")
endif()

get_filename_component(TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER
    "${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}" ABSOLUTE)
get_filename_component(TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST
    "${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}" ABSOLUTE)
file(SHA256 "${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}" TPA_YOLOV8N_HEADER_SHA256)
file(SHA256 "${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}" TPA_YOLOV8N_MANIFEST_SHA256)
if (NOT TPA_YOLOV8N_HEADER_SHA256 STREQUAL TPA_YOLOV8N_EXPECT_HEADER_SHA256)
    message(FATAL_ERROR
        "YOLOv8n external weight header checksum mismatch. Got ${TPA_YOLOV8N_HEADER_SHA256}; expected ${TPA_YOLOV8N_EXPECT_HEADER_SHA256}")
endif()
if (NOT TPA_YOLOV8N_MANIFEST_SHA256 STREQUAL TPA_YOLOV8N_EXPECT_MANIFEST_SHA256)
    message(FATAL_ERROR
        "YOLOv8n generated manifest checksum mismatch. Got ${TPA_YOLOV8N_MANIFEST_SHA256}; expected ${TPA_YOLOV8N_EXPECT_MANIFEST_SHA256}")
endif()

set(TPA_YOLOV8N_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}")
set(TPA_YOLOV8N_EXTERNAL_WEIGHTS_EXTERN_HEADER
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_external_detect_c2f_weights_extern.h")
set(TPA_PLANNER_ROOT "${TPA_ROOT_DIR}/planner")
set(TPA_PLANNER_PYTHONPATH "${TPA_PLANNER_ROOT}/src")
if (DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
    set(TPA_PLANNER_PYTHONPATH "${TPA_PLANNER_PYTHONPATH}:$ENV{PYTHONPATH}")
endif()
set(TPA_PLANNER_EXTRACT "${TPA_PLANNER_ROOT}/src/tpa_planner/extract_process_metadata.py")
set(TPA_PLANNER_PLAN "${TPA_PLANNER_ROOT}/src/tpa_planner/plan_program.py")
set(TPA_PLANNER_MAP "${TPA_PLANNER_ROOT}/src/tpa_planner/map_program.py")
set(TPA_PLANNER_COMMON "${TPA_PLANNER_ROOT}/src/tpa_planner/common.py")
set(TPA_PLANNER_EDGE "${TPA_PLANNER_ROOT}/src/tpa_planner/edge_plan.py")
set(TPA_YOLOV8N_MACHINE_JSON "${TPA_ROOT_DIR}/machines/erbium.json")
if (TPA_PLATFORM_ID STREQUAL "etsoc1")
    set(TPA_YOLOV8N_MACHINE_JSON "${TPA_ROOT_DIR}/machines/etsoc1.json")
    if (TPA_ETSOC1_NR_SHIRES LESS 32)
        message(FATAL_ERROR
            "ET-SoC-1 YOLOv8n mapping uses the full-card machine description. "
            "Configure with -DTPA_ETSOC1_NR_SHIRES=32 so every mapped hart "
            "has compiled TPA runtime queues.")
    endif()
endif()

set(PYTHON "" CACHE FILEPATH "Python interpreter")
if (NOT PYTHON)
    set(Python3_FIND_VIRTUALENV FIRST)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(PYTHON "${Python3_EXECUTABLE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${TPA_PLANNER_PYTHONPATH}"
        "${PYTHON}" -c
        "import tpa_planner.extract_process_metadata; import tpa_planner.plan_program; import tpa_planner.map_program"
    RESULT_VARIABLE TPA_PLANNER_IMPORT_RESULT
    ERROR_VARIABLE TPA_PLANNER_IMPORT_ERROR
    ERROR_STRIP_TRAILING_WHITESPACE
)
if (TPA_PLANNER_IMPORT_RESULT)
    message(FATAL_ERROR
        "TPA planner package is not available for ${PYTHON}.\n"
        "Install it with:\n"
        "  ${PYTHON} -m pip install -e ${TPA_PLANNER_ROOT}\n"
        "or reconfigure with:\n"
        "  -DPYTHON=/path/to/python\n"
        "Import error:\n"
        "${TPA_PLANNER_IMPORT_ERROR}")
endif()

set(TPA_YOLOV8N_METADATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/metadata")
set(TPA_YOLOV8N_REPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/planner")

function(yolov8n_add_process_metadata TARGET_NAME MANIFEST_PATH OUT_VAR)
    set(JSON_OUT "${TPA_YOLOV8N_METADATA_DIR}/${TARGET_NAME}.json")

    add_custom_command(
        OUTPUT "${JSON_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${TPA_YOLOV8N_METADATA_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${TPA_PLANNER_PYTHONPATH}"
            "${PYTHON}" -m tpa_planner.extract_process_metadata
            --label "${TARGET_NAME}"
            --manifest "${MANIFEST_PATH}"
            --output "${JSON_OUT}"
            --objects $<TARGET_OBJECTS:${TARGET_NAME}>
        DEPENDS
            ${TARGET_NAME}
            "${MANIFEST_PATH}"
            "${TPA_PLANNER_EXTRACT}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(${TARGET_NAME}_metadata_json DEPENDS "${JSON_OUT}")
    add_dependencies(${TARGET_NAME}_metadata_json ${TARGET_NAME})
    set_target_properties(${TARGET_NAME} PROPERTIES TPA_METADATA_JSON "${JSON_OUT}")
    set(${OUT_VAR} "${JSON_OUT}" PARENT_SCOPE)
endfunction()

function(yolov8n_add_plan_report NAME PROGRAM_PATH PLACEMENT_PATH)
    set(options)
    set(oneValueArgs OUT_VAR)
    set(multiValueArgs PROCESS_TARGETS)
    cmake_parse_arguments(YV8NPLAN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(PROCESS_JSONS)
    set(PROCESS_JSON_TARGETS)
    foreach (PROC_TARGET IN LISTS YV8NPLAN_PROCESS_TARGETS)
        get_target_property(PROC_JSON ${PROC_TARGET} TPA_METADATA_JSON)
        if (NOT PROC_JSON)
            message(FATAL_ERROR "No TPA_METADATA_JSON property for ${PROC_TARGET}")
        endif()
        list(APPEND PROCESS_JSONS "${PROC_JSON}")
        list(APPEND PROCESS_JSON_TARGETS ${PROC_TARGET}_metadata_json)
    endforeach()

    set(JSON_OUT "${TPA_YOLOV8N_REPORT_DIR}/${NAME}.json")
    add_custom_command(
        OUTPUT "${JSON_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${TPA_YOLOV8N_REPORT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${TPA_PLANNER_PYTHONPATH}"
            "${PYTHON}" -m tpa_planner.plan_program
            --program "${PROGRAM_PATH}"
            --placement "${PLACEMENT_PATH}"
            --output "${JSON_OUT}"
            --process-jsons ${PROCESS_JSONS}
        DEPENDS
            ${PROCESS_JSON_TARGETS}
            "${PROGRAM_PATH}"
            "${PLACEMENT_PATH}"
            "${TPA_PLANNER_PLAN}"
            "${TPA_PLANNER_COMMON}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(${NAME}_planner_json DEPENDS "${JSON_OUT}")
    set(${YV8NPLAN_OUT_VAR} "${JSON_OUT}" PARENT_SCOPE)
endfunction()

function(yolov8n_add_mapped_program NAME PROGRAM_PATH)
    set(options)
    set(oneValueArgs OUT_PLACEMENT_VAR OUT_SCRATCH_HEADER_VAR OUT_EDGE_HEADER_VAR OUT_REPORT_VAR OUT_MAPPED_PROGRAM_VAR NUM_MINIONS HART MEMORY_BUDGET_BYTES COMPUTE_COSTS_JSON MACHINE_JSON)
    set(multiValueArgs PROCESS_TARGETS)
    cmake_parse_arguments(YV8NMAP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT YV8NMAP_NUM_MINIONS)
        set(YV8NMAP_NUM_MINIONS 8)
    endif()
    if (NOT YV8NMAP_HART)
        set(YV8NMAP_HART h0)
    endif()

    set(PROCESS_JSONS)
    set(PROCESS_JSON_TARGETS)
    foreach (PROC_TARGET IN LISTS YV8NMAP_PROCESS_TARGETS)
        get_target_property(PROC_JSON ${PROC_TARGET} TPA_METADATA_JSON)
        if (NOT PROC_JSON)
            message(FATAL_ERROR "No TPA_METADATA_JSON property for ${PROC_TARGET}")
        endif()
        list(APPEND PROCESS_JSONS "${PROC_JSON}")
        list(APPEND PROCESS_JSON_TARGETS ${PROC_TARGET}_metadata_json)
    endforeach()

    set(REPORT_OUT "${TPA_YOLOV8N_REPORT_DIR}/${NAME}_map.json")
    set(MAPPED_PROGRAM_OUT "${TPA_YOLOV8N_REPORT_DIR}/${NAME}_mapped_program.json")
    set(PLACEMENT_OUT "${TPA_YOLOV8N_REPORT_DIR}/${NAME}.place")
    set(SCRATCH_HEADER_OUT "${TPA_YOLOV8N_REPORT_DIR}/${NAME}_scratch_config.h")
    set(EDGE_HEADER_OUT "${TPA_YOLOV8N_REPORT_DIR}/${NAME}_edge_config.h")

    set(MAP_ARGS
        --program "${PROGRAM_PATH}"
        --output "${REPORT_OUT}"
        --mapped-program-out "${MAPPED_PROGRAM_OUT}"
        --placement-out "${PLACEMENT_OUT}"
        --scratch-header-out "${SCRATCH_HEADER_OUT}"
        --edge-config-header-out "${EDGE_HEADER_OUT}"
        --num-minions "${YV8NMAP_NUM_MINIONS}"
        --hart "${YV8NMAP_HART}"
        --process-jsons ${PROCESS_JSONS}
    )
    if (YV8NMAP_COMPUTE_COSTS_JSON)
        list(APPEND MAP_ARGS --compute-costs-json "${YV8NMAP_COMPUTE_COSTS_JSON}")
    endif()
    if (YV8NMAP_MACHINE_JSON)
        list(APPEND MAP_ARGS --machine-json "${YV8NMAP_MACHINE_JSON}")
    endif()
    if (YV8NMAP_MEMORY_BUDGET_BYTES)
        list(APPEND MAP_ARGS --memory-budget-bytes "${YV8NMAP_MEMORY_BUDGET_BYTES}")
    endif()

    add_custom_command(
        OUTPUT
            "${REPORT_OUT}"
            "${MAPPED_PROGRAM_OUT}"
            "${PLACEMENT_OUT}"
            "${SCRATCH_HEADER_OUT}"
            "${EDGE_HEADER_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${TPA_YOLOV8N_REPORT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=${TPA_PLANNER_PYTHONPATH}"
            "${PYTHON}" -m tpa_planner.map_program ${MAP_ARGS}
        DEPENDS
            ${PROCESS_JSON_TARGETS}
            "${PROGRAM_PATH}"
            "${TPA_PLANNER_MAP}"
            "${TPA_PLANNER_COMMON}"
            "${TPA_PLANNER_EDGE}"
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

    if (YV8NMAP_OUT_PLACEMENT_VAR)
        set(${YV8NMAP_OUT_PLACEMENT_VAR} "${PLACEMENT_OUT}" PARENT_SCOPE)
    endif()
    if (YV8NMAP_OUT_SCRATCH_HEADER_VAR)
        set(${YV8NMAP_OUT_SCRATCH_HEADER_VAR} "${SCRATCH_HEADER_OUT}" PARENT_SCOPE)
    endif()
    if (YV8NMAP_OUT_EDGE_HEADER_VAR)
        set(${YV8NMAP_OUT_EDGE_HEADER_VAR} "${EDGE_HEADER_OUT}" PARENT_SCOPE)
    endif()
    if (YV8NMAP_OUT_REPORT_VAR)
        set(${YV8NMAP_OUT_REPORT_VAR} "${REPORT_OUT}" PARENT_SCOPE)
    endif()
    if (YV8NMAP_OUT_MAPPED_PROGRAM_VAR)
        set(${YV8NMAP_OUT_MAPPED_PROGRAM_VAR} "${MAPPED_PROGRAM_OUT}" PARENT_SCOPE)
    endif()
endfunction()

add_tpa_process(
    NAME tpa_yolov8n_p5_detect_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_detect.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_detect.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_p5_detect_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_p5_detect_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_detect.tpm"
    TPA_YOLOV8N_P5_DETECT_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_p5_detect_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_detect.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_detect.place"
    PROCESS_TARGETS tpa_yolov8n_p5_detect_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_p5_detect_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_detect.tpp"
    PROCESS_TARGETS tpa_yolov8n_p5_detect_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_P5_DETECT_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_P5_DETECT_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_P5_DETECT_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_P5_DETECT_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_P5_DETECT_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_detect_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
)

add_tpa_program_test(
    NAME tpa_yolov8n_p5_detect
    PROCESSES tpa_yolov8n_p5_detect_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_detect.tpp"
    PLACEMENT "${TPA_YOLOV8N_P5_DETECT_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_P5_DETECT_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_p5_detect.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(tpa_yolov8n_p5_detect.elf tpa_yolov8n_p5_detect_map_mapped_program)
target_compile_definitions(
    tpa_yolov8n_p5_detect.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_P5_DETECT_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_p5_c2f_detect_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_p5_c2f_detect_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_p5_c2f_detect_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect.tpm"
    TPA_YOLOV8N_P5_C2F_DETECT_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_p5_c2f_detect_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect.place"
    PROCESS_TARGETS tpa_yolov8n_p5_c2f_detect_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_p5_c2f_detect_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect.tpp"
    PROCESS_TARGETS tpa_yolov8n_p5_c2f_detect_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_P5_C2F_DETECT_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_P5_C2F_DETECT_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_P5_C2F_DETECT_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_P5_C2F_DETECT_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_P5_C2F_DETECT_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 4
)

add_tpa_program_test(
    NAME tpa_yolov8n_p5_c2f_detect
    PROCESSES tpa_yolov8n_p5_c2f_detect_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect.tpp"
    PLACEMENT "${TPA_YOLOV8N_P5_C2F_DETECT_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_P5_C2F_DETECT_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_p5_c2f_detect.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_p5_c2f_detect.elf
    tpa_yolov8n_p5_c2f_detect_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_p5_c2f_detect.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_P5_C2F_DETECT_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_p5_dense_c2f_detect_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_p5_dense_c2f_detect_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_p5_dense_c2f_detect_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect.tpm"
    TPA_YOLOV8N_P5_DENSE_C2F_DETECT_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_p5_dense_c2f_detect_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect.place"
    PROCESS_TARGETS tpa_yolov8n_p5_dense_c2f_detect_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_p5_dense_c2f_detect_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect.tpp"
    PROCESS_TARGETS tpa_yolov8n_p5_dense_c2f_detect_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_P5_DENSE_C2F_DETECT_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_P5_DENSE_C2F_DETECT_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_P5_DENSE_C2F_DETECT_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_P5_DENSE_C2F_DETECT_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_P5_DENSE_C2F_DETECT_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 4
)

add_tpa_program_test(
    NAME tpa_yolov8n_p5_dense_c2f_detect
    PROCESSES tpa_yolov8n_p5_dense_c2f_detect_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect.tpp"
    PLACEMENT "${TPA_YOLOV8N_P5_DENSE_C2F_DETECT_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_P5_DENSE_C2F_DETECT_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_p5_dense_c2f_detect.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_p5_dense_c2f_detect.elf
    tpa_yolov8n_p5_dense_c2f_detect_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_p5_dense_c2f_detect.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_P5_DENSE_C2F_DETECT_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_p3_c2f_detect_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_p3_c2f_detect_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_p3_c2f_detect_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect.tpm"
    TPA_YOLOV8N_P3_C2F_DETECT_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_p3_c2f_detect_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect.place"
    PROCESS_TARGETS tpa_yolov8n_p3_c2f_detect_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_p3_c2f_detect_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect.tpp"
    PROCESS_TARGETS tpa_yolov8n_p3_c2f_detect_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_P3_C2F_DETECT_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_P3_C2F_DETECT_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_P3_C2F_DETECT_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_P3_C2F_DETECT_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_P3_C2F_DETECT_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 4
)

add_tpa_program_test(
    NAME tpa_yolov8n_p3_c2f_detect
    PROCESSES tpa_yolov8n_p3_c2f_detect_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect.tpp"
    PLACEMENT "${TPA_YOLOV8N_P3_C2F_DETECT_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_P3_C2F_DETECT_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_p3_c2f_detect.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_p3_c2f_detect.elf
    tpa_yolov8n_p3_c2f_detect_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_p3_c2f_detect.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_P3_C2F_DETECT_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_p3_dense_c2f_detect_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_p3_dense_c2f_detect_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_p3_dense_c2f_detect_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect.tpm"
    TPA_YOLOV8N_P3_DENSE_C2F_DETECT_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_p3_dense_c2f_detect_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect.place"
    PROCESS_TARGETS tpa_yolov8n_p3_dense_c2f_detect_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_p3_dense_c2f_detect_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect.tpp"
    PROCESS_TARGETS tpa_yolov8n_p3_dense_c2f_detect_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_P3_DENSE_C2F_DETECT_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_P3_DENSE_C2F_DETECT_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_P3_DENSE_C2F_DETECT_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_P3_DENSE_C2F_DETECT_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_P3_DENSE_C2F_DETECT_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 4
)

add_tpa_program_test(
    NAME tpa_yolov8n_p3_dense_c2f_detect
    PROCESSES tpa_yolov8n_p3_dense_c2f_detect_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect.tpp"
    PLACEMENT "${TPA_YOLOV8N_P3_DENSE_C2F_DETECT_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_P3_DENSE_C2F_DETECT_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_p3_dense_c2f_detect.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_p3_dense_c2f_detect.elf
    tpa_yolov8n_p3_dense_c2f_detect_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_p3_dense_c2f_detect.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_P3_DENSE_C2F_DETECT_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_p4_c2f_detect_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_p4_c2f_detect_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_p4_c2f_detect_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect.tpm"
    TPA_YOLOV8N_P4_C2F_DETECT_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_p4_c2f_detect_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect.place"
    PROCESS_TARGETS tpa_yolov8n_p4_c2f_detect_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_p4_c2f_detect_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect.tpp"
    PROCESS_TARGETS tpa_yolov8n_p4_c2f_detect_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_P4_C2F_DETECT_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_P4_C2F_DETECT_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_P4_C2F_DETECT_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_P4_C2F_DETECT_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_P4_C2F_DETECT_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 4
)

add_tpa_program_test(
    NAME tpa_yolov8n_p4_c2f_detect
    PROCESSES tpa_yolov8n_p4_c2f_detect_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect.tpp"
    PLACEMENT "${TPA_YOLOV8N_P4_C2F_DETECT_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_P4_C2F_DETECT_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_p4_c2f_detect.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_p4_c2f_detect.elf
    tpa_yolov8n_p4_c2f_detect_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_p4_c2f_detect.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_P4_C2F_DETECT_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_p4_dense_c2f_detect_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_p4_dense_c2f_detect_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_p4_dense_c2f_detect_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.tpm"
    TPA_YOLOV8N_P4_DENSE_C2F_DETECT_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_p4_dense_c2f_detect_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.place"
    PROCESS_TARGETS tpa_yolov8n_p4_dense_c2f_detect_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_p4_dense_c2f_detect_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.tpp"
    PROCESS_TARGETS tpa_yolov8n_p4_dense_c2f_detect_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_P4_DENSE_C2F_DETECT_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_P4_DENSE_C2F_DETECT_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_P4_DENSE_C2F_DETECT_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_P4_DENSE_C2F_DETECT_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_P4_DENSE_C2F_DETECT_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 4
)

add_tpa_program_test(
    NAME tpa_yolov8n_p4_dense_c2f_detect
    PROCESSES tpa_yolov8n_p4_dense_c2f_detect_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.tpp"
    PLACEMENT "${TPA_YOLOV8N_P4_DENSE_C2F_DETECT_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_P4_DENSE_C2F_DETECT_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_p4_dense_c2f_detect.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_p4_dense_c2f_detect.elf
    tpa_yolov8n_p4_dense_c2f_detect_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_p4_dense_c2f_detect.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_P4_DENSE_C2F_DETECT_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_p4_p5_neck_tail_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_p5_neck_tail.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream_weights.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_p5_neck_tail.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_p4_p5_neck_tail_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_EXTERN_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_SOURCE_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_p4_p5_neck_tail_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_p5_neck_tail.tpm"
    TPA_YOLOV8N_P4_P5_NECK_TAIL_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_p4_p5_neck_tail_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_p5_neck_tail.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_p5_neck_tail.place"
    PROCESS_TARGETS tpa_yolov8n_p4_p5_neck_tail_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_p4_p5_neck_tail_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_p5_neck_tail.tpp"
    PROCESS_TARGETS tpa_yolov8n_p4_p5_neck_tail_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_P4_P5_NECK_TAIL_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_P4_P5_NECK_TAIL_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_P4_P5_NECK_TAIL_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_P4_P5_NECK_TAIL_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_P4_P5_NECK_TAIL_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_p5_neck_tail_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 4
)

add_tpa_program_test(
    NAME tpa_yolov8n_p4_p5_neck_tail
    PROCESSES tpa_yolov8n_p4_p5_neck_tail_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_p5_neck_tail.tpp"
    PLACEMENT "${TPA_YOLOV8N_P4_P5_NECK_TAIL_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_P4_P5_NECK_TAIL_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_p4_p5_neck_tail.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_p4_p5_neck_tail.elf
    tpa_yolov8n_p4_p5_neck_tail_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_p4_p5_neck_tail.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_P4_P5_NECK_TAIL_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_dense_c2f_detect_downstream_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_dense_c2f_detect_downstream.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream_weights.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_dense_c2f_detect.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_dense_c2f_detect.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_dense_c2f_detect.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_dense_c2f_detect_downstream.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_dense_c2f_detect_downstream_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_EXTERN_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_SOURCE_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_dense_c2f_detect_downstream_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_dense_c2f_detect_downstream.tpm"
    TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_dense_c2f_detect_downstream_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_dense_c2f_detect_downstream.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_dense_c2f_detect_downstream.place"
    PROCESS_TARGETS tpa_yolov8n_dense_c2f_detect_downstream_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_dense_c2f_detect_downstream_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_dense_c2f_detect_downstream.tpp"
    PROCESS_TARGETS tpa_yolov8n_dense_c2f_detect_downstream_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_dense_c2f_detect_downstream_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 4
)

add_tpa_program_test(
    NAME tpa_yolov8n_dense_c2f_detect_downstream
    PROCESSES tpa_yolov8n_dense_c2f_detect_downstream_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_dense_c2f_detect_downstream.tpp"
    PLACEMENT "${TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_dense_c2f_detect_downstream.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_dense_c2f_detect_downstream.elf
    tpa_yolov8n_dense_c2f_detect_downstream_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_dense_c2f_detect_downstream.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_DENSE_C2F_DETECT_DOWNSTREAM_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_c2f_detect_downstream_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream_weights.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p3_c2f_detect.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p4_c2f_detect.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_c2f_detect.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_c2f_detect_downstream_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_EXTERN_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_SOURCE_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_c2f_detect_downstream_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream.tpm"
    TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_c2f_detect_downstream_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream.place"
    PROCESS_TARGETS tpa_yolov8n_c2f_detect_downstream_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_c2f_detect_downstream_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream.tpp"
    PROCESS_TARGETS tpa_yolov8n_c2f_detect_downstream_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 3
)

add_tpa_program_test(
    NAME tpa_yolov8n_c2f_detect_downstream
    PROCESSES tpa_yolov8n_c2f_detect_downstream_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_c2f_detect_downstream.tpp"
    PLACEMENT "${TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_c2f_detect_downstream.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_c2f_detect_downstream.elf
    tpa_yolov8n_c2f_detect_downstream_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_c2f_detect_downstream.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_C2F_DETECT_DOWNSTREAM_SCRATCH_HEADER}\"
)

add_tpa_process(
    NAME tpa_yolov8n_detect_downstream_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_detect_downstream.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_detect_downstream.c"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_compile_definitions(
    tpa_yolov8n_detect_downstream_proc
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
)
yolov8n_add_process_metadata(
    tpa_yolov8n_detect_downstream_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_detect_downstream.tpm"
    TPA_YOLOV8N_DETECT_DOWNSTREAM_JSON
)

yolov8n_add_plan_report(
    tpa_yolov8n_detect_downstream_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_detect_downstream.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_detect_downstream.place"
    PROCESS_TARGETS tpa_yolov8n_detect_downstream_proc
)

yolov8n_add_mapped_program(
    tpa_yolov8n_detect_downstream_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_detect_downstream.tpp"
    PROCESS_TARGETS tpa_yolov8n_detect_downstream_proc
    OUT_PLACEMENT_VAR TPA_YOLOV8N_DETECT_DOWNSTREAM_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV8N_DETECT_DOWNSTREAM_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV8N_DETECT_DOWNSTREAM_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV8N_DETECT_DOWNSTREAM_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV8N_DETECT_DOWNSTREAM_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_detect_downstream_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV8N_MACHINE_JSON}"
    NUM_MINIONS 3
)

add_tpa_program_test(
    NAME tpa_yolov8n_detect_downstream
    PROCESSES tpa_yolov8n_detect_downstream_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_detect_downstream.tpp"
    PLACEMENT "${TPA_YOLOV8N_DETECT_DOWNSTREAM_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV8N_DETECT_DOWNSTREAM_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV8N_LDS}"
    INCLUDES ${TPA_YOLOV8N_INCLUDES}
)
target_sources(
    tpa_yolov8n_detect_downstream.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov8n_p5_arena.c"
)
add_dependencies(
    tpa_yolov8n_detect_downstream.elf
    tpa_yolov8n_detect_downstream_map_mapped_program
)
target_compile_definitions(
    tpa_yolov8n_detect_downstream.elf
    PRIVATE
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}\"
        TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=\"${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}\"
        YV8N_P5_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV8N_DETECT_DOWNSTREAM_SCRATCH_HEADER}\"
)

set(TPA_YOLOV8N_ERBIUM_EMU "${ERBIUM_EMU}")
if (NOT TPA_YOLOV8N_ERBIUM_EMU AND DEFINED ET_ROOT)
    set(TPA_YOLOV8N_ERBIUM_EMU "${ET_ROOT}/bin/erbium_emu")
endif()
if (NOT TPA_YOLOV8N_ERBIUM_EMU AND DEFINED ET_PLATFORM_PATH)
    set(TPA_YOLOV8N_ERBIUM_EMU "${ET_PLATFORM_PATH}/bin/erbium_emu")
endif()
if (NOT TPA_YOLOV8N_ERBIUM_EMU)
    set(TPA_YOLOV8N_ERBIUM_EMU "/opt/et/bin/erbium_emu")
endif()

include(CTest)
if (TPA_PLATFORM_ID STREQUAL "erbium" AND BUILD_TESTING AND EXISTS "${TPA_YOLOV8N_ERBIUM_EMU}")
    add_test(
        NAME tpa_yolov8n_p5_detect
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_p5_detect.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_p5_detect.out
            -DMAX_CYCLES=400000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolov8n_p5_c2f_detect
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_p5_c2f_detect.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_p5_c2f_detect.out
            -DMAX_CYCLES=1200000000
            -DMINIONS=0xf
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolov8n_p5_dense_c2f_detect
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_p5_dense_c2f_detect.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_p5_dense_c2f_detect.out
            -DMAX_CYCLES=12000000000
            -DMINIONS=0xf
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    set_tests_properties(tpa_yolov8n_p5_dense_c2f_detect PROPERTIES TIMEOUT 3600)
    add_test(
        NAME tpa_yolov8n_p4_dense_c2f_detect
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_p4_dense_c2f_detect.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_p4_dense_c2f_detect.out
            -DMAX_CYCLES=18000000000
            -DMINIONS=0xf
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    set_tests_properties(tpa_yolov8n_p4_dense_c2f_detect PROPERTIES TIMEOUT 7200)
    add_test(
        NAME tpa_yolov8n_p4_p5_neck_tail
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_p4_p5_neck_tail.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_p4_p5_neck_tail.out
            -DMAX_CYCLES=30000000000
            -DMINIONS=0xf
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    set_tests_properties(tpa_yolov8n_p4_p5_neck_tail PROPERTIES TIMEOUT 9000)
    add_test(
        NAME tpa_yolov8n_p3_dense_c2f_detect
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_p3_dense_c2f_detect.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_p3_dense_c2f_detect.out
            -DMAX_CYCLES=24000000000
            -DMINIONS=0xf
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    set_tests_properties(tpa_yolov8n_p3_dense_c2f_detect PROPERTIES TIMEOUT 9000)
    add_test(
        NAME tpa_yolov8n_dense_c2f_detect_downstream
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_dense_c2f_detect_downstream.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_dense_c2f_detect_downstream.out
            -DMAX_CYCLES=42000000000
            -DMINIONS=0xf
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    set_tests_properties(tpa_yolov8n_dense_c2f_detect_downstream PROPERTIES TIMEOUT 14400)
    add_test(
        NAME tpa_yolov8n_p3_c2f_detect
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_p3_c2f_detect.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_p3_c2f_detect.out
            -DMAX_CYCLES=1800000000
            -DMINIONS=0xf
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolov8n_p4_c2f_detect
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_p4_c2f_detect.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_p4_c2f_detect.out
            -DMAX_CYCLES=1200000000
            -DMINIONS=0xf
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolov8n_c2f_detect_downstream
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_c2f_detect_downstream.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_c2f_detect_downstream.out
            -DMAX_CYCLES=3000000000
            -DMINIONS=0x7
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolov8n_detect_downstream
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${TPA_YOLOV8N_ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolov8n_detect_downstream.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolov8n_detect_downstream.out
            -DMAX_CYCLES=800000000
            -DMINIONS=0x7
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
endif()
