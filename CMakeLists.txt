cmake_minimum_required(VERSION 3.20)

project(tpa-structured-superbuild LANGUAGES NONE)

option(TPA_HOST_SMOKE_TEST_DOUBLE "Build host-only smoke-test doubles instead of ET device projects" OFF)

if (TPA_HOST_SMOKE_TEST_DOUBLE)
    message(STATUS
        "TPA_HOST_SMOKE_TEST_DOUBLE=ON builds host test doubles only. "
        "It does not validate Erbium or ET-SoC-1 device integration.")
    enable_language(C)
    include(CTest)
    add_subdirectory(tpa)
    return()
endif()

if (DEFINED ET_ROOT AND NOT "${ET_ROOT}" STREQUAL "")
    set(ET_PLATFORM_PATH "${ET_ROOT}" CACHE PATH "ET SDK install root" FORCE)
endif()

set(TPA_ET_HINTS)
if (DEFINED ET_PLATFORM_PATH AND NOT "${ET_PLATFORM_PATH}" STREQUAL "")
    list(APPEND TPA_ET_HINTS "${ET_PLATFORM_PATH}")
endif()
if (DEFINED ET_ROOT AND NOT "${ET_ROOT}" STREQUAL "")
    list(APPEND TPA_ET_HINTS "${ET_ROOT}")
endif()
list(APPEND TPA_ET_HINTS ${CMAKE_MODULE_PATH})
list(APPEND TPA_ET_HINTS "/opt/et")
list(REMOVE_DUPLICATES TPA_ET_HINTS)

unset(TPA_ET_PROJECT_FUNCTIONS CACHE)
find_file(TPA_ET_PROJECT_FUNCTIONS
    NAMES ProjectFunctions.cmake
    HINTS ${TPA_ET_HINTS}
    PATH_SUFFIXES lib/cmake/aifoundry-utils aifoundry-utils
    NO_DEFAULT_PATH
)

if (NOT TPA_ET_PROJECT_FUNCTIONS)
    message(FATAL_ERROR
        "Could not find ProjectFunctions.cmake. Configure with "
        "-DET_ROOT=/path/to/et-platform or -DET_PLATFORM_PATH=/path/to/et-platform. "
        "For local syntax-only checks use -DTPA_HOST_SMOKE_TEST_DOUBLE=ON.")
endif()

if (NOT DEFINED ET_PLATFORM_PATH OR "${ET_PLATFORM_PATH}" STREQUAL "")
    get_filename_component(TPA_ET_CMAKE_DIR "${TPA_ET_PROJECT_FUNCTIONS}" DIRECTORY)
    if (TPA_ET_CMAKE_DIR MATCHES "/lib/cmake/aifoundry-utils$")
        get_filename_component(TPA_ET_PLATFORM_PATH "${TPA_ET_CMAKE_DIR}/../../.." ABSOLUTE)
    else()
        get_filename_component(TPA_ET_PLATFORM_PATH "${TPA_ET_CMAKE_DIR}" ABSOLUTE)
    endif()
    set(ET_PLATFORM_PATH "${TPA_ET_PLATFORM_PATH}" CACHE PATH "ET SDK install root")
endif()

list(PREPEND CMAKE_MODULE_PATH
    "${ET_PLATFORM_PATH}/lib/cmake/aifoundry-utils"
    "${ET_PLATFORM_PATH}/lib/cmake"
    "${ET_PLATFORM_PATH}/lib/cmake/cmake-modules"
)
list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)

include("${TPA_ET_PROJECT_FUNCTIONS}")

option(BUILD_TPA_DEVICE "Configure and build TPA device targets" ON)
option(BUILD_TPA_HOST "Configure TPA host integration project" ON)
option(BUILD_TPA_YOLOV8N "Build structured YOLOv8n external-header targets" OFF)
set(TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER "" CACHE FILEPATH "External generated YOLOv8n weight header")
set(TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST "" CACHE FILEPATH "External generated YOLOv8n weight manifest")
if (BUILD_TPA_YOLOV8N)
    if (NOT TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER)
        message(FATAL_ERROR
            "BUILD_TPA_YOLOV8N=ON requires -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=/path/to/yolov8n_external_detect_c2f_weights.h")
    endif()
    if (NOT TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST)
        message(FATAL_ERROR
            "BUILD_TPA_YOLOV8N=ON requires -DTPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=/path/to/yolov8n_external_detect_c2f_generated_manifest.json")
    endif()
endif()
set(TPA_PLATFORM "erbium" CACHE STRING "TPA device platform")
set_property(CACHE TPA_PLATFORM PROPERTY STRINGS erbium etsoc1)
string(TOLOWER "${TPA_PLATFORM}" TPA_PLATFORM_ID)
set(TPA_ETSOC1_NR_SHIRES 1 CACHE STRING "Number of ET-SoC-1 shires with compiled TPA queues")

if (BUILD_TPA_DEVICE)
    set(TPA_DEVICE_CMAKE_ARGS
        "-DTPA_ROOT_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
        "-DET_ROOT=${ET_PLATFORM_PATH}"
        "-DET_PLATFORM_PATH=${ET_PLATFORM_PATH}"
        "-DTPA_PLATFORM=${TPA_PLATFORM}"
        "-DTPA_ETSOC1_NR_SHIRES=${TPA_ETSOC1_NR_SHIRES}"
    )
    list(APPEND TPA_DEVICE_CMAKE_ARGS "-DBUILD_TPA_YOLOV8N=${BUILD_TPA_YOLOV8N}")
    if (DEFINED TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER AND NOT "${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}" STREQUAL "")
        list(APPEND TPA_DEVICE_CMAKE_ARGS "-DTPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER=${TPA_YOLOV8N_EXTERNAL_WEIGHTS_HEADER}")
    endif()
    if (DEFINED TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST AND NOT "${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}" STREQUAL "")
        list(APPEND TPA_DEVICE_CMAKE_ARGS "-DTPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST=${TPA_YOLOV8N_EXTERNAL_WEIGHTS_MANIFEST}")
    endif()
    if (DEFINED PYTHON AND NOT "${PYTHON}" STREQUAL "")
        list(APPEND TPA_DEVICE_CMAKE_ARGS "-DPYTHON=${PYTHON}")
    endif()
    list(JOIN TPA_DEVICE_CMAKE_ARGS " " TPA_DEVICE_CMAKE_ARGS_STRING)

    DeviceProjectNoInstall(tpa-device "${TPA_DEVICE_CMAKE_ARGS_STRING}")
    set_property(TARGET tpa-device PROPERTY EXCLUDE_FROM_ALL TRUE)
    ExternalProject_Add_StepTargets(tpa-device configure)

    function(tpa_forward_device_target target)
        add_custom_target(${target}
            COMMAND "${CMAKE_COMMAND}" --build "${tpa-device_BINARY_DIR}" --target "${target}"
            DEPENDS tpa-device-configure
            USES_TERMINAL
            VERBATIM
        )
    endfunction()

    tpa_forward_device_target(tpa_core)
    tpa_forward_device_target(tpa_empty.elf)
    tpa_forward_device_target(tpa_pipe_demo.elf)
    tpa_forward_device_target(tpa_tensor_matmul.elf)
    tpa_forward_device_target(tpa_fast_attention_map_mapped_program)
    tpa_forward_device_target(tpa_fast_attention.elf)
    tpa_forward_device_target(tpa_fast_attention_serial.elf)
    tpa_forward_device_target(tpa_yolov5n_downstream_plan_planner_json)
    tpa_forward_device_target(tpa_yolov5n_downstream_map_mapped_program)
    tpa_forward_device_target(tpa_yolov5n_downstream.elf)
    if (BUILD_TPA_YOLOV8N)
        tpa_forward_device_target(tpa_yolov8n_p5_detect_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_p5_detect_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_p5_detect.elf)
        tpa_forward_device_target(tpa_yolov8n_p5_c2f_detect_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_p5_c2f_detect_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_p5_c2f_detect.elf)
        tpa_forward_device_target(tpa_yolov8n_p5_dense_c2f_detect_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_p5_dense_c2f_detect_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_p5_dense_c2f_detect.elf)
        tpa_forward_device_target(tpa_yolov8n_p3_c2f_detect_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_p3_c2f_detect_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_p3_c2f_detect.elf)
        tpa_forward_device_target(tpa_yolov8n_p3_dense_c2f_detect_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_p3_dense_c2f_detect_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_p3_dense_c2f_detect.elf)
        tpa_forward_device_target(tpa_yolov8n_p4_c2f_detect_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_p4_c2f_detect_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_p4_c2f_detect.elf)
        tpa_forward_device_target(tpa_yolov8n_p4_dense_c2f_detect_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_p4_dense_c2f_detect_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_p4_dense_c2f_detect.elf)
        tpa_forward_device_target(tpa_yolov8n_c2f_detect_downstream_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_c2f_detect_downstream_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_c2f_detect_downstream.elf)
        tpa_forward_device_target(tpa_yolov8n_detect_downstream_plan_planner_json)
        tpa_forward_device_target(tpa_yolov8n_detect_downstream_map_mapped_program)
        tpa_forward_device_target(tpa_yolov8n_detect_downstream.elf)
    endif()
    tpa_forward_device_target(tpa_depth_sad_cost_5x5.elf)
    tpa_forward_device_target(tpa_depth_sad_argmin_16x8.elf)
    tpa_forward_device_target(tpa_depth_sad_stripe_96x16.elf)
    tpa_forward_device_target(tpa_stereo_sad_hand_plan_planner_json)
    tpa_forward_device_target(tpa_stereo_sad_map_mapped_program)
    tpa_forward_device_target(tpa_stereo_sad.elf)
    tpa_forward_device_target(tpa_stereo_sad_mapped.elf)
    tpa_forward_device_target(tpa_queue_basic.elf)
    tpa_forward_device_target(tpa_queue_yield.elf)
    tpa_forward_device_target(tpa_queue_many.elf)
    tpa_forward_device_target(tpa_queue_wake.elf)
    tpa_forward_device_target(tpa_msg_same_send_first.elf)
    tpa_forward_device_target(tpa_msg_same_recv_first.elf)
    tpa_forward_device_target(tpa_msg_cross_send_first.elf)
    tpa_forward_device_target(tpa_msg_cross_recv_first.elf)
    tpa_forward_device_target(tpa_msg_fabric_send_first.elf)
    tpa_forward_device_target(tpa_msg_fabric_recv_first.elf)
    tpa_forward_device_target(tpa_msg_bench_100.elf)
    tpa_forward_device_target(tpa_msg_fabric_race.elf)
    tpa_forward_device_target(tpa_msg_edge_storage_direct_send_first.elf)
    tpa_forward_device_target(tpa_msg_edge_storage_direct_recv_first.elf)
    tpa_forward_device_target(tpa_msg_edge_storage_local_send_first.elf)
    tpa_forward_device_target(tpa_msg_edge_storage_local_recv_first.elf)
    tpa_forward_device_target(tpa_msg_edge_storage_fabric_send_first.elf)
    tpa_forward_device_target(tpa_msg_edge_storage_fabric_recv_first.elf)
    tpa_forward_device_target(tpa_negative_expected_fail.elf)
    if (TPA_PLATFORM_ID STREQUAL "erbium")
        tpa_forward_device_target(tpa_yolo_cbs_l40.elf)
        tpa_forward_device_target(tpa_yolo_sppf_l31_32.elf)
        tpa_forward_device_target(tpa_yolov8n_c2f_block.elf)
        tpa_forward_device_target(tpa_yolov8n_detect_dfl.elf)
    endif()
endif()

if (BUILD_TPA_HOST)
    set(TPA_HOST_CMAKE_ARGS
        "-DET_ROOT=${ET_PLATFORM_PATH}"
        "-DET_PLATFORM_PATH=${ET_PLATFORM_PATH}"
    )
    list(JOIN TPA_HOST_CMAKE_ARGS " " TPA_HOST_CMAKE_ARGS_STRING)

    HostProjectNoInstall(tpa-host "${TPA_HOST_CMAKE_ARGS_STRING}")
    set_property(TARGET tpa-host PROPERTY EXCLUDE_FROM_ALL TRUE)
    ExternalProject_Add_StepTargets(tpa-host configure)

    add_custom_target(tpa_host_tools
        COMMAND "${CMAKE_COMMAND}" --build "${tpa-host_BINARY_DIR}" --target tpa_host_tools
        DEPENDS tpa-host-configure
        USES_TERMINAL
        VERBATIM
    )
endif()

set(TPA_DEFAULT_COMMANDS)
set(TPA_DEFAULT_DEPS)
if (BUILD_TPA_DEVICE)
    list(APPEND TPA_DEFAULT_COMMANDS
        COMMAND "${CMAKE_COMMAND}" --build "${tpa-device_BINARY_DIR}" --target tpa_core
    )
    list(APPEND TPA_DEFAULT_DEPS tpa-device-configure)
endif()

if (BUILD_TPA_HOST)
    list(APPEND TPA_DEFAULT_COMMANDS
        COMMAND "${CMAKE_COMMAND}" --build "${tpa-host_BINARY_DIR}" --target tpa_host_tools
    )
    list(APPEND TPA_DEFAULT_DEPS tpa-host-configure)
endif()

if (TPA_DEFAULT_COMMANDS)
    if (TPA_PLATFORM_ID STREQUAL "etsoc1")
        set(TPA_DEFAULT_TARGET tpa_etsoc1)
    else()
        set(TPA_DEFAULT_TARGET tpa_core_all)
    endif()

    add_custom_target(${TPA_DEFAULT_TARGET} ALL
        ${TPA_DEFAULT_COMMANDS}
        DEPENDS ${TPA_DEFAULT_DEPS}
        USES_TERMINAL
        VERBATIM
    )

    if (NOT TPA_DEFAULT_TARGET STREQUAL "tpa_core")
        add_custom_target(tpa_core_default DEPENDS ${TPA_DEFAULT_TARGET})
    endif()
endif()
