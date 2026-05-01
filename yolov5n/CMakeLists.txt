set(TPA_YOLOV5N_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")
set(TPA_YOLOV5N_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${TPA_ROOT_DIR}/tests/yolo"
)
set(TPA_PLANNER_ROOT "${TPA_ROOT_DIR}/planner")
set(TPA_PLANNER_EXTRACT "${TPA_PLANNER_ROOT}/src/tpa_planner/extract_process_metadata.py")
set(TPA_PLANNER_PLAN "${TPA_PLANNER_ROOT}/src/tpa_planner/plan_program.py")
set(TPA_PLANNER_MAP "${TPA_PLANNER_ROOT}/src/tpa_planner/map_program.py")
set(TPA_PLANNER_COMMON "${TPA_PLANNER_ROOT}/src/tpa_planner/common.py")
set(TPA_PLANNER_EDGE "${TPA_PLANNER_ROOT}/src/tpa_planner/edge_plan.py")
set(TPA_YOLOV5N_MACHINE_JSON "${TPA_ROOT_DIR}/machines/erbium.json")
if (TPA_PLATFORM_ID STREQUAL "etsoc1")
    set(TPA_YOLOV5N_MACHINE_JSON "${TPA_ROOT_DIR}/machines/etsoc1.json")
    if (TPA_ETSOC1_NR_SHIRES LESS 32)
        message(FATAL_ERROR
            "ET-SoC-1 YOLO mapping uses the full-card machine description. "
            "Configure with -DTPA_ETSOC1_NR_SHIRES=32 so every mapped hart "
            "has compiled TPA runtime queues."
        )
    endif()
endif()
get_filename_component(TPA_ERBIUM_EMU_BUILD_DIR "${ERBIUM_EMU}" DIRECTORY)
get_filename_component(TPA_ERBIUM_EMU_SOURCE_DIR "${TPA_ERBIUM_EMU_BUILD_DIR}" DIRECTORY)
set(TPA_ERBIUM_EMU_LIB "${TPA_ERBIUM_EMU_BUILD_DIR}/libsw-erbium.a")
set(TPA_ERBIUM_EMU_BUILD_INCLUDE "${TPA_ERBIUM_EMU_BUILD_DIR}/sw-sysemu/include/sw-sysemu")
set(TPA_ERBIUM_EMU_EXPORT_INCLUDE "${TPA_ERBIUM_EMU_SOURCE_DIR}/sw-sysemu/include")
set(TPA_ERBIUM_EMU_ROOT_INCLUDE "${TPA_ERBIUM_EMU_SOURCE_DIR}")
set(TPA_ERBIUM_EMU_SYS_INCLUDE "${TPA_ERBIUM_EMU_SOURCE_DIR}/sys_emu")

set(PYTHON "" CACHE FILEPATH "Python interpreter")
if (NOT PYTHON)
    set(Python3_FIND_VIRTUALENV FIRST)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(PYTHON "${Python3_EXECUTABLE}")
endif()

execute_process(
    COMMAND "${PYTHON}" -c
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
        "${TPA_PLANNER_IMPORT_ERROR}"
    )
endif()

set(TPA_YOLOV5N_METADATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/metadata")
set(TPA_YOLOV5N_REPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/planner")

function(yolov5n_add_process_metadata TARGET_NAME MANIFEST_PATH OUT_VAR)
    set(JSON_OUT "${TPA_YOLOV5N_METADATA_DIR}/${TARGET_NAME}.json")

    add_custom_command(
        OUTPUT "${JSON_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${TPA_YOLOV5N_METADATA_DIR}"
        COMMAND "${PYTHON}" -m tpa_planner.extract_process_metadata
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

function(yolov5n_add_plan_report NAME PROGRAM_PATH PLACEMENT_PATH)
    set(options)
    set(oneValueArgs OUT_VAR)
    set(multiValueArgs PROCESS_TARGETS)
    cmake_parse_arguments(YV5NPLAN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(PROCESS_JSONS)
    set(PROCESS_JSON_TARGETS)
    foreach (PROC_TARGET IN LISTS YV5NPLAN_PROCESS_TARGETS)
        get_target_property(PROC_JSON ${PROC_TARGET} TPA_METADATA_JSON)
        if (NOT PROC_JSON)
            message(FATAL_ERROR "No TPA_METADATA_JSON property for ${PROC_TARGET}")
        endif()
        list(APPEND PROCESS_JSONS "${PROC_JSON}")
        list(APPEND PROCESS_JSON_TARGETS ${PROC_TARGET}_metadata_json)
    endforeach()

    set(JSON_OUT "${TPA_YOLOV5N_REPORT_DIR}/${NAME}.json")
    add_custom_command(
        OUTPUT "${JSON_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${TPA_YOLOV5N_REPORT_DIR}"
        COMMAND "${PYTHON}" -m tpa_planner.plan_program
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
    set(${YV5NPLAN_OUT_VAR} "${JSON_OUT}" PARENT_SCOPE)
endfunction()

function(yolov5n_add_mapped_program NAME PROGRAM_PATH)
    set(options)
    set(oneValueArgs OUT_PLACEMENT_VAR OUT_SCRATCH_HEADER_VAR OUT_EDGE_HEADER_VAR OUT_REPORT_VAR OUT_MAPPED_PROGRAM_VAR NUM_MINIONS HART MEMORY_BUDGET_BYTES COMPUTE_COSTS_JSON MACHINE_JSON)
    set(multiValueArgs PROCESS_TARGETS)
    cmake_parse_arguments(YV5NMAP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (NOT YV5NMAP_NUM_MINIONS)
        set(YV5NMAP_NUM_MINIONS 8)
    endif()
    if (NOT YV5NMAP_HART)
        set(YV5NMAP_HART h0)
    endif()

    set(PROCESS_JSONS)
    set(PROCESS_JSON_TARGETS)
    foreach (PROC_TARGET IN LISTS YV5NMAP_PROCESS_TARGETS)
        get_target_property(PROC_JSON ${PROC_TARGET} TPA_METADATA_JSON)
        if (NOT PROC_JSON)
            message(FATAL_ERROR "No TPA_METADATA_JSON property for ${PROC_TARGET}")
        endif()
        list(APPEND PROCESS_JSONS "${PROC_JSON}")
        list(APPEND PROCESS_JSON_TARGETS ${PROC_TARGET}_metadata_json)
    endforeach()

    set(REPORT_OUT "${TPA_YOLOV5N_REPORT_DIR}/${NAME}_map.json")
    set(MAPPED_PROGRAM_OUT "${TPA_YOLOV5N_REPORT_DIR}/${NAME}_mapped_program.json")
    set(PLACEMENT_OUT "${TPA_YOLOV5N_REPORT_DIR}/${NAME}.place")
    set(SCRATCH_HEADER_OUT "${TPA_YOLOV5N_REPORT_DIR}/${NAME}_scratch_config.h")
    set(EDGE_HEADER_OUT "${TPA_YOLOV5N_REPORT_DIR}/${NAME}_edge_config.h")

    set(MAP_ARGS
        --program "${PROGRAM_PATH}"
        --output "${REPORT_OUT}"
        --mapped-program-out "${MAPPED_PROGRAM_OUT}"
        --placement-out "${PLACEMENT_OUT}"
        --scratch-header-out "${SCRATCH_HEADER_OUT}"
        --edge-config-header-out "${EDGE_HEADER_OUT}"
        --num-minions "${YV5NMAP_NUM_MINIONS}"
        --hart "${YV5NMAP_HART}"
        --process-jsons ${PROCESS_JSONS}
    )
    if (YV5NMAP_COMPUTE_COSTS_JSON)
        list(APPEND MAP_ARGS --compute-costs-json "${YV5NMAP_COMPUTE_COSTS_JSON}")
    endif()
    if (YV5NMAP_MACHINE_JSON)
        list(APPEND MAP_ARGS --machine-json "${YV5NMAP_MACHINE_JSON}")
    endif()
    if (YV5NMAP_MEMORY_BUDGET_BYTES)
        list(APPEND MAP_ARGS --memory-budget-bytes "${YV5NMAP_MEMORY_BUDGET_BYTES}")
    endif()

    add_custom_command(
        OUTPUT
            "${REPORT_OUT}"
            "${MAPPED_PROGRAM_OUT}"
            "${PLACEMENT_OUT}"
            "${SCRATCH_HEADER_OUT}"
            "${EDGE_HEADER_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${TPA_YOLOV5N_REPORT_DIR}"
        COMMAND "${PYTHON}" -m tpa_planner.map_program ${MAP_ARGS}
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

    if (YV5NMAP_OUT_PLACEMENT_VAR)
        set(${YV5NMAP_OUT_PLACEMENT_VAR} "${PLACEMENT_OUT}" PARENT_SCOPE)
    endif()
    if (YV5NMAP_OUT_SCRATCH_HEADER_VAR)
        set(${YV5NMAP_OUT_SCRATCH_HEADER_VAR} "${SCRATCH_HEADER_OUT}" PARENT_SCOPE)
    endif()
    if (YV5NMAP_OUT_REPORT_VAR)
        set(${YV5NMAP_OUT_REPORT_VAR} "${REPORT_OUT}" PARENT_SCOPE)
    endif()
    if (YV5NMAP_OUT_MAPPED_PROGRAM_VAR)
        set(${YV5NMAP_OUT_MAPPED_PROGRAM_VAR} "${MAPPED_PROGRAM_OUT}" PARENT_SCOPE)
    endif()
    if (YV5NMAP_OUT_EDGE_HEADER_VAR)
        set(${YV5NMAP_OUT_EDGE_HEADER_VAR} "${EDGE_HEADER_OUT}" PARENT_SCOPE)
    endif()
endfunction()

add_tpa_process(
    NAME tpa_yolov5n_sources_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_sources.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_sources.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_sources_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_sources.tpm"
    TPA_YOLOV5N_SOURCES_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_full_sources_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_full_sources.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_full_sources.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_full_sources_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_full_sources.tpm"
    TPA_YOLOV5N_FULL_SOURCES_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_fork_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_forks.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_forks.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_fork_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_forks.tpm"
    TPA_YOLOV5N_FORK_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_demo_sink_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_demo_sink.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_demo_sink.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_demo_sink_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_demo_sink.tpm"
    TPA_YOLOV5N_DEMO_SINK_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p0_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p0.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p0.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p0_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p0.tpm"
    TPA_YOLOV5N_P0_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p1_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p1.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p1.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p1_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p1.tpm"
    TPA_YOLOV5N_P1_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p2_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p2.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p2.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p2_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p2.tpm"
    TPA_YOLOV5N_P2_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p3_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p3.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p3.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p3_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p3.tpm"
    TPA_YOLOV5N_P3_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p4_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p4.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p4.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p4_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p4.tpm"
    TPA_YOLOV5N_P4_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p5_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p5.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p5.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p5_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p5.tpm"
    TPA_YOLOV5N_P5_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p6_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p6.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p6.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p6_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p6.tpm"
    TPA_YOLOV5N_P6_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p7_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p7.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p7.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p7_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p7.tpm"
    TPA_YOLOV5N_P7_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p8_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p8.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p8.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p8_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p8.tpm"
    TPA_YOLOV5N_P8_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p9_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p9.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p9.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p9_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p9.tpm"
    TPA_YOLOV5N_P9_JSON
)

add_tpa_process(
    NAME tpa_yolov5n_p10_proc
    MANIFEST "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p10.tpm"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p10.c"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)
yolov5n_add_process_metadata(
    tpa_yolov5n_p10_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_p10.tpm"
    TPA_YOLOV5N_P10_JSON
)

foreach (PROC_TARGET IN ITEMS
    tpa_yolov5n_sources_proc
    tpa_yolov5n_full_sources_proc
    tpa_yolov5n_fork_proc
    tpa_yolov5n_demo_sink_proc
    tpa_yolov5n_p0_proc
    tpa_yolov5n_p1_proc
    tpa_yolov5n_p2_proc
    tpa_yolov5n_p3_proc
    tpa_yolov5n_p4_proc
    tpa_yolov5n_p5_proc
    tpa_yolov5n_p6_proc
    tpa_yolov5n_p7_proc
    tpa_yolov5n_p8_proc
    tpa_yolov5n_p9_proc
    tpa_yolov5n_p10_proc
)
    target_compile_definitions(${PROC_TARGET} PRIVATE YV5N_EDGE_OWNERSHIP=1)
endforeach()

yolov5n_add_mapped_program(
    tpa_yolov5n_downstream_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_downstream.tpp"
    PROCESS_TARGETS
        tpa_yolov5n_sources_proc
        tpa_yolov5n_fork_proc
        tpa_yolov5n_p4_proc
        tpa_yolov5n_p5_proc
        tpa_yolov5n_p6_proc
        tpa_yolov5n_p7_proc
        tpa_yolov5n_p8_proc
        tpa_yolov5n_p9_proc
        tpa_yolov5n_p10_proc
    OUT_PLACEMENT_VAR TPA_YOLOV5N_DOWNSTREAM_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV5N_DOWNSTREAM_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV5N_DOWNSTREAM_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV5N_DOWNSTREAM_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV5N_DOWNSTREAM_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV5N_MACHINE_JSON}"
)

yolov5n_add_mapped_program(
    tpa_yolov5n_full_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_full.tpp"
    PROCESS_TARGETS
        tpa_yolov5n_full_sources_proc
        tpa_yolov5n_fork_proc
        tpa_yolov5n_p0_proc
        tpa_yolov5n_p1_proc
        tpa_yolov5n_p2_proc
        tpa_yolov5n_p3_proc
        tpa_yolov5n_p4_proc
        tpa_yolov5n_p5_proc
        tpa_yolov5n_p6_proc
        tpa_yolov5n_p7_proc
        tpa_yolov5n_p8_proc
        tpa_yolov5n_p9_proc
        tpa_yolov5n_p10_proc
    OUT_PLACEMENT_VAR TPA_YOLOV5N_FULL_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV5N_FULL_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV5N_FULL_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV5N_FULL_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV5N_FULL_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV5N_MACHINE_JSON}"
)

yolov5n_add_mapped_program(
    tpa_yolov5n_demo_map
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_demo.tpp"
    PROCESS_TARGETS
        tpa_yolov5n_full_sources_proc
        tpa_yolov5n_fork_proc
        tpa_yolov5n_demo_sink_proc
        tpa_yolov5n_p0_proc
        tpa_yolov5n_p1_proc
        tpa_yolov5n_p2_proc
        tpa_yolov5n_p3_proc
        tpa_yolov5n_p4_proc
        tpa_yolov5n_p5_proc
        tpa_yolov5n_p6_proc
        tpa_yolov5n_p7_proc
        tpa_yolov5n_p8_proc
        tpa_yolov5n_p9_proc
        tpa_yolov5n_p10_proc
    OUT_PLACEMENT_VAR TPA_YOLOV5N_DEMO_MAPPED_PLACEMENT
    OUT_SCRATCH_HEADER_VAR TPA_YOLOV5N_DEMO_SCRATCH_HEADER
    OUT_EDGE_HEADER_VAR TPA_YOLOV5N_DEMO_EDGE_HEADER
    OUT_REPORT_VAR TPA_YOLOV5N_DEMO_MAP_REPORT
    OUT_MAPPED_PROGRAM_VAR TPA_YOLOV5N_DEMO_MAPPED_PROGRAM
    COMPUTE_COSTS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_compute_costs.json"
    MACHINE_JSON "${TPA_YOLOV5N_MACHINE_JSON}"
)

add_tpa_program_test(
    NAME tpa_yolov5n_downstream
    PROCESSES
        tpa_yolov5n_sources_proc
        tpa_yolov5n_fork_proc
        tpa_yolov5n_p4_proc
        tpa_yolov5n_p5_proc
        tpa_yolov5n_p6_proc
        tpa_yolov5n_p7_proc
        tpa_yolov5n_p8_proc
        tpa_yolov5n_p9_proc
        tpa_yolov5n_p10_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_downstream.tpp"
    PLACEMENT "${TPA_YOLOV5N_DOWNSTREAM_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV5N_DOWNSTREAM_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV5N_LDS}"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)

add_tpa_program_test(
    NAME tpa_yolov5n_full
    PROCESSES
        tpa_yolov5n_full_sources_proc
        tpa_yolov5n_fork_proc
        tpa_yolov5n_p0_proc
        tpa_yolov5n_p1_proc
        tpa_yolov5n_p2_proc
        tpa_yolov5n_p3_proc
        tpa_yolov5n_p4_proc
        tpa_yolov5n_p5_proc
        tpa_yolov5n_p6_proc
        tpa_yolov5n_p7_proc
        tpa_yolov5n_p8_proc
        tpa_yolov5n_p9_proc
        tpa_yolov5n_p10_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_full.tpp"
    PLACEMENT "${TPA_YOLOV5N_FULL_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV5N_FULL_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV5N_LDS}"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)

add_tpa_program_test(
    NAME tpa_yolov5n_demo
    PROCESSES
        tpa_yolov5n_full_sources_proc
        tpa_yolov5n_fork_proc
        tpa_yolov5n_demo_sink_proc
        tpa_yolov5n_p0_proc
        tpa_yolov5n_p1_proc
        tpa_yolov5n_p2_proc
        tpa_yolov5n_p3_proc
        tpa_yolov5n_p4_proc
        tpa_yolov5n_p5_proc
        tpa_yolov5n_p6_proc
        tpa_yolov5n_p7_proc
        tpa_yolov5n_p8_proc
        tpa_yolov5n_p9_proc
        tpa_yolov5n_p10_proc
    PROGRAM "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_demo.tpp"
    PLACEMENT "${TPA_YOLOV5N_DEMO_MAPPED_PLACEMENT}"
    EDGE_CONFIG_HEADER "${TPA_YOLOV5N_DEMO_EDGE_HEADER}"
    LINKER_SCRIPT "${TPA_YOLOV5N_LDS}"
    INCLUDES ${TPA_YOLOV5N_INCLUDES}
)

target_sources(
    tpa_yolov5n_downstream.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_common.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_luts.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_residual_luts.c"
)
target_link_libraries(tpa_yolov5n_downstream.elf PRIVATE m)
add_dependencies(tpa_yolov5n_downstream.elf tpa_yolov5n_downstream_map_mapped_program)
target_compile_definitions(
    tpa_yolov5n_downstream.elf
    PRIVATE
        YV5N_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV5N_DOWNSTREAM_SCRATCH_HEADER}\"
)
target_sources(
    tpa_yolov5n_full.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_common.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_luts.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_residual_luts.c"
)
target_link_libraries(tpa_yolov5n_full.elf PRIVATE m)
add_dependencies(tpa_yolov5n_full.elf tpa_yolov5n_full_map_mapped_program)
target_compile_definitions(
    tpa_yolov5n_full.elf
    PRIVATE
        YV5N_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV5N_FULL_SCRATCH_HEADER}\"
)
target_sources(
    tpa_yolov5n_demo.elf
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_common.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_luts.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_residual_luts.c"
)
target_link_libraries(tpa_yolov5n_demo.elf PRIVATE m)
add_dependencies(tpa_yolov5n_demo.elf tpa_yolov5n_demo_map_mapped_program)
target_compile_definitions(
    tpa_yolov5n_demo.elf
    PRIVATE
        YV5N_SCRATCH_CONFIG_HEADER=\"${TPA_YOLOV5N_DEMO_SCRATCH_HEADER}\"
)

if (TPA_PLATFORM_ID STREQUAL "erbium")
    set(TPA_YOLOV5N_DEMO_HOST_BIN "${CMAKE_CURRENT_BINARY_DIR}/yolo_demo")
    add_custom_command(
        OUTPUT "${TPA_YOLOV5N_DEMO_HOST_BIN}"
        COMMAND /usr/bin/c++
            -std=c++17
            -g
            -DERBIUM
            -DSYS_EMU
            -I${CMAKE_CURRENT_SOURCE_DIR}
            -I${TPA_ERBIUM_EMU_ROOT_INCLUDE}
            -I${TPA_ERBIUM_EMU_SYS_INCLUDE}
            -I${TPA_ERBIUM_EMU_BUILD_INCLUDE}
            -I${TPA_ERBIUM_EMU_EXPORT_INCLUDE}
            ${CMAKE_CURRENT_SOURCE_DIR}/yolo_demo.cpp
            -o ${TPA_YOLOV5N_DEMO_HOST_BIN}
            ${TPA_ERBIUM_EMU_LIB}
            /usr/lib/x86_64-linux-gnu/liblz4.a
            /usr/lib/x86_64-linux-gnu/libglog.so
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/yolo_demo.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_demo.h"
            tpa_yolov5n_demo.elf
            "${TPA_ERBIUM_EMU_LIB}"
        VERBATIM
    )
    add_custom_target(yolo_demo_host DEPENDS "${TPA_YOLOV5N_DEMO_HOST_BIN}")
endif()

yolov5n_add_plan_report(
    tpa_yolov5n_downstream_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_downstream.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_downstream.place"
    PROCESS_TARGETS
        tpa_yolov5n_sources_proc
        tpa_yolov5n_fork_proc
        tpa_yolov5n_p4_proc
        tpa_yolov5n_p5_proc
        tpa_yolov5n_p6_proc
        tpa_yolov5n_p7_proc
        tpa_yolov5n_p8_proc
        tpa_yolov5n_p9_proc
        tpa_yolov5n_p10_proc
)

yolov5n_add_plan_report(
    tpa_yolov5n_full_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_full.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/yolov5n_full.place"
    PROCESS_TARGETS
        tpa_yolov5n_full_sources_proc
        tpa_yolov5n_fork_proc
        tpa_yolov5n_p0_proc
        tpa_yolov5n_p1_proc
        tpa_yolov5n_p2_proc
        tpa_yolov5n_p3_proc
        tpa_yolov5n_p4_proc
        tpa_yolov5n_p5_proc
        tpa_yolov5n_p6_proc
        tpa_yolov5n_p7_proc
        tpa_yolov5n_p8_proc
        tpa_yolov5n_p9_proc
        tpa_yolov5n_p10_proc
)
