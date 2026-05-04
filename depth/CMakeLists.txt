set(TPA_DEPTH_DEMO_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")
set(TPA_DEPTH_PLANNER_ROOT "${TPA_ROOT_DIR}/planner")
set(TPA_DEPTH_PLANNER_PYTHONPATH "${TPA_DEPTH_PLANNER_ROOT}/src")
if (DEFINED ENV{PYTHONPATH} AND NOT "$ENV{PYTHONPATH}" STREQUAL "")
    set(TPA_DEPTH_PLANNER_PYTHONPATH
        "${TPA_DEPTH_PLANNER_PYTHONPATH}:$ENV{PYTHONPATH}")
endif()
set(TPA_DEPTH_PLANNER_EXTRACT
    "${TPA_DEPTH_PLANNER_ROOT}/src/tpa_planner/extract_process_metadata.py")
set(TPA_DEPTH_PLANNER_PLAN
    "${TPA_DEPTH_PLANNER_ROOT}/src/tpa_planner/plan_program.py")
set(TPA_DEPTH_PLANNER_MAP
    "${TPA_DEPTH_PLANNER_ROOT}/src/tpa_planner/map_program.py")
set(TPA_DEPTH_PLANNER_COMMON
    "${TPA_DEPTH_PLANNER_ROOT}/src/tpa_planner/common.py")
set(TPA_DEPTH_PLANNER_EDGE
    "${TPA_DEPTH_PLANNER_ROOT}/src/tpa_planner/edge_plan.py")
set(TPA_DEPTH_METADATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/metadata")
set(TPA_DEPTH_REPORT_DIR "${CMAKE_CURRENT_BINARY_DIR}/planner")
set(TPA_DEPTH_STEREO_SAD_COMPUTE_COSTS_JSON
    "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad_compute_costs.json")
set(TPA_DEPTH_STEREO_SAD_MACHINE_JSON "${TPA_ROOT_DIR}/machines/erbium.json")

set(PYTHON "" CACHE FILEPATH "Python interpreter")
if (NOT PYTHON)
    set(Python3_FIND_VIRTUALENV FIRST)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(PYTHON "${Python3_EXECUTABLE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env
        "PYTHONPATH=${TPA_DEPTH_PLANNER_PYTHONPATH}"
        "${PYTHON}" -c
        "import tpa_planner.extract_process_metadata; import tpa_planner.plan_program; import tpa_planner.map_program"
    RESULT_VARIABLE TPA_DEPTH_PLANNER_IMPORT_RESULT
    ERROR_VARIABLE TPA_DEPTH_PLANNER_IMPORT_ERROR
    ERROR_STRIP_TRAILING_WHITESPACE
)
if (TPA_DEPTH_PLANNER_IMPORT_RESULT)
    message(FATAL_ERROR
        "TPA planner package is not available for ${PYTHON}.\n"
        "Install it with:\n"
        "  ${PYTHON} -m pip install -e ${TPA_DEPTH_PLANNER_ROOT}\n"
        "or reconfigure with:\n"
        "  -DPYTHON=/path/to/python\n"
        "Import error:\n"
        "${TPA_DEPTH_PLANNER_IMPORT_ERROR}"
    )
endif()

function(depth_add_process_metadata TARGET_NAME MANIFEST_PATH)
    set(JSON_OUT "${TPA_DEPTH_METADATA_DIR}/${TARGET_NAME}.json")

    add_custom_command(
        OUTPUT "${JSON_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${TPA_DEPTH_METADATA_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "PYTHONPATH=${TPA_DEPTH_PLANNER_PYTHONPATH}"
            "${PYTHON}" -m tpa_planner.extract_process_metadata
            --label "${TARGET_NAME}"
            --manifest "${MANIFEST_PATH}"
            --output "${JSON_OUT}"
            --objects $<TARGET_OBJECTS:${TARGET_NAME}>
        DEPENDS
            ${TARGET_NAME}
            "${MANIFEST_PATH}"
            "${TPA_DEPTH_PLANNER_EXTRACT}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(${TARGET_NAME}_metadata_json DEPENDS "${JSON_OUT}")
    add_dependencies(${TARGET_NAME}_metadata_json ${TARGET_NAME})
    set_target_properties(${TARGET_NAME} PROPERTIES TPA_METADATA_JSON "${JSON_OUT}")
endfunction()

function(depth_add_plan_report NAME PROGRAM_PATH PLACEMENT_PATH)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs PROCESS_TARGETS)
    cmake_parse_arguments(DEPTHPLAN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(PROCESS_JSONS)
    set(PROCESS_JSON_TARGETS)
    foreach (PROC_TARGET IN LISTS DEPTHPLAN_PROCESS_TARGETS)
        get_target_property(PROC_JSON ${PROC_TARGET} TPA_METADATA_JSON)
        if (NOT PROC_JSON)
            message(FATAL_ERROR "No TPA_METADATA_JSON property for ${PROC_TARGET}")
        endif()
        list(APPEND PROCESS_JSONS "${PROC_JSON}")
        list(APPEND PROCESS_JSON_TARGETS ${PROC_TARGET}_metadata_json)
    endforeach()

    set(JSON_OUT "${TPA_DEPTH_REPORT_DIR}/${NAME}.json")
    add_custom_command(
        OUTPUT "${JSON_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${TPA_DEPTH_REPORT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "PYTHONPATH=${TPA_DEPTH_PLANNER_PYTHONPATH}"
            "${PYTHON}" -m tpa_planner.plan_program
            --program "${PROGRAM_PATH}"
            --placement "${PLACEMENT_PATH}"
            --output "${JSON_OUT}"
            --process-jsons ${PROCESS_JSONS}
        DEPENDS
            ${PROCESS_JSON_TARGETS}
            "${PROGRAM_PATH}"
            "${PLACEMENT_PATH}"
            "${TPA_DEPTH_PLANNER_PLAN}"
            "${TPA_DEPTH_PLANNER_COMMON}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_custom_target(${NAME}_planner_json DEPENDS "${JSON_OUT}")
endfunction()

function(depth_add_mapped_program NAME PROGRAM_PATH)
    set(options)
    set(oneValueArgs COMPUTE_COSTS_JSON MACHINE_JSON)
    set(multiValueArgs PROCESS_TARGETS)
    cmake_parse_arguments(DEPTHMAP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(PROCESS_JSONS)
    set(PROCESS_JSON_TARGETS)
    foreach (PROC_TARGET IN LISTS DEPTHMAP_PROCESS_TARGETS)
        get_target_property(PROC_JSON ${PROC_TARGET} TPA_METADATA_JSON)
        if (NOT PROC_JSON)
            message(FATAL_ERROR "No TPA_METADATA_JSON property for ${PROC_TARGET}")
        endif()
        list(APPEND PROCESS_JSONS "${PROC_JSON}")
        list(APPEND PROCESS_JSON_TARGETS ${PROC_TARGET}_metadata_json)
    endforeach()

    set(REPORT_OUT "${TPA_DEPTH_REPORT_DIR}/${NAME}_map.json")
    set(MAPPED_PROGRAM_OUT "${TPA_DEPTH_REPORT_DIR}/${NAME}_mapped_program.json")
    set(PLACEMENT_OUT "${TPA_DEPTH_REPORT_DIR}/${NAME}.place")
    set(SCRATCH_HEADER_OUT "${TPA_DEPTH_REPORT_DIR}/${NAME}_scratch_config.h")
    set(EDGE_HEADER_OUT "${TPA_DEPTH_REPORT_DIR}/${NAME}_edge_config.h")

    set(MAP_ARGS
        --program "${PROGRAM_PATH}"
        --output "${REPORT_OUT}"
        --mapped-program-out "${MAPPED_PROGRAM_OUT}"
        --placement-out "${PLACEMENT_OUT}"
        --scratch-header-out "${SCRATCH_HEADER_OUT}"
        --edge-config-header-out "${EDGE_HEADER_OUT}"
        --process-jsons ${PROCESS_JSONS}
    )
    set(MAP_DEPS)
    if (DEPTHMAP_COMPUTE_COSTS_JSON)
        list(APPEND MAP_ARGS --compute-costs-json "${DEPTHMAP_COMPUTE_COSTS_JSON}")
        list(APPEND MAP_DEPS "${DEPTHMAP_COMPUTE_COSTS_JSON}")
    endif()
    if (DEPTHMAP_MACHINE_JSON)
        list(APPEND MAP_ARGS --machine-json "${DEPTHMAP_MACHINE_JSON}")
        list(APPEND MAP_DEPS "${DEPTHMAP_MACHINE_JSON}")
    endif()

    add_custom_command(
        OUTPUT
            "${REPORT_OUT}"
            "${MAPPED_PROGRAM_OUT}"
            "${PLACEMENT_OUT}"
            "${SCRATCH_HEADER_OUT}"
            "${EDGE_HEADER_OUT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${TPA_DEPTH_REPORT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E env
            "PYTHONPATH=${TPA_DEPTH_PLANNER_PYTHONPATH}"
            "${PYTHON}" -m tpa_planner.map_program ${MAP_ARGS}
        DEPENDS
            ${PROCESS_JSON_TARGETS}
            "${PROGRAM_PATH}"
            ${MAP_DEPS}
            "${TPA_DEPTH_PLANNER_MAP}"
            "${TPA_DEPTH_PLANNER_COMMON}"
            "${TPA_DEPTH_PLANNER_EDGE}"
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
endfunction()

add_tpa_process(
    NAME tpa_stereo_sad_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.c"
)
depth_add_process_metadata(
    tpa_stereo_sad_proc
    "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.tpm"
)

depth_add_plan_report(
    tpa_stereo_sad_hand_plan
    "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.tpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.place"
    PROCESS_TARGETS
        tpa_stereo_sad_proc
)

depth_add_mapped_program(
    tpa_stereo_sad_map
    "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.tpp"
    PROCESS_TARGETS
        tpa_stereo_sad_proc
    COMPUTE_COSTS_JSON
        "${TPA_DEPTH_STEREO_SAD_COMPUTE_COSTS_JSON}"
    MACHINE_JSON
        "${TPA_DEPTH_STEREO_SAD_MACHINE_JSON}"
)

add_tpa_program(
    NAME tpa_stereo_sad
    PROCESSES
        tpa_stereo_sad_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.place"
    LINKER_SCRIPT
        "${TPA_DEPTH_DEMO_LDS}"
)

if (TPA_PLATFORM_ID STREQUAL "erbium")
    find_program(TPA_ERBIUM_EMU
        NAMES erbium_emu
        HINTS
            "${ET_ROOT}/bin"
            "${ET_PLATFORM_PATH}/bin"
            "/opt/et/bin"
        PATH_SUFFIXES bin
    )

    if (TPA_ERBIUM_EMU)
        enable_testing()
        add_test(
            NAME tpa_stereo_sad_erbium
            COMMAND "${CMAKE_COMMAND}"
                -DEMU=${TPA_ERBIUM_EMU}
                -DELF=$<TARGET_FILE:tpa_stereo_sad.elf>
                -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_stereo_sad_erbium.log
                -DMAX_CYCLES=100000000
                -DMINIONS=0x1f
                -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
        )
    endif()
endif()
