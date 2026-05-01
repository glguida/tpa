set(TPA_YOLO_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")

set(YOLO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(YOLO_SUBGRAPH_TPM "${YOLO_GEN_DIR}/yolo_subgraph_p4_p5_p6.tpm")
set(YOLO_SUBGRAPH_TPP "${YOLO_GEN_DIR}/yolo_subgraph_p4_p5_p6.tpp")
set(YOLO_SUBGRAPH_PLACE "${YOLO_GEN_DIR}/yolo_subgraph_p4_p5_p6.place")

file(MAKE_DIRECTORY "${YOLO_GEN_DIR}")

file(WRITE "${YOLO_SUBGRAPH_TPM}" [=[# Written by CMake. Do not hand-edit.

pdef yolo_subgraph_src_p5_pdef user 408 yolo_subgraph_src_p5_start 0
port yolo_subgraph_src_p5_pdef 0 out

pdef yolo_subgraph_src_p4_pdef user 409 yolo_subgraph_src_p4_start 0
port yolo_subgraph_src_p4_pdef 0 out

pdef yolo_subgraph_p4_pdef user 410 yolo_subgraph_p4_start 512000
port yolo_subgraph_p4_pdef 0 in
port yolo_subgraph_p4_pdef 1 in
port yolo_subgraph_p4_pdef 2 out

pdef yolo_subgraph_src_p3_pdef user 411 yolo_subgraph_src_p3_start 0
port yolo_subgraph_src_p3_pdef 0 out

pdef yolo_subgraph_p5_pdef user 412 yolo_subgraph_p5_start 1024000
port yolo_subgraph_p5_pdef 0 in
port yolo_subgraph_p5_pdef 1 in
port yolo_subgraph_p5_pdef 2 out

pdef yolo_subgraph_p6_pdef user 413 yolo_subgraph_p6_start 512000
port yolo_subgraph_p6_pdef 0 in
]=])

file(WRITE "${YOLO_SUBGRAPH_TPP}" [=[# Written by CMake. Do not hand-edit.

inst 408 yolo_subgraph_src_p5_pdef
inst 409 yolo_subgraph_src_p4_pdef
inst 410 yolo_subgraph_p4_pdef
inst 411 yolo_subgraph_src_p3_pdef
inst 412 yolo_subgraph_p5_pdef
inst 413 yolo_subgraph_p6_pdef

conn 408 0 410 0 51200
conn 409 0 410 1 204800
conn 410 2 412 0 102400
conn 411 0 412 1 409600
conn 412 2 413 0 409600
]=])

file(WRITE "${YOLO_SUBGRAPH_PLACE}" [=[# Written by CMake. Do not hand-edit.

408 6
409 4
410 8
411 2
412 10
413 12
]=])

add_tpa_process(
    NAME tpa_yolo_cbs_l40_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/cbs_l40.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/cbs_l40.c"
)

add_tpa_program(
    NAME tpa_yolo_cbs_l40
    PROCESSES
        tpa_yolo_cbs_l40_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/cbs_l40.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/cbs_l40.place"
    LINKER_SCRIPT
        "${TPA_YOLO_LDS}"
)

add_tpa_process(
    NAME tpa_yolo_bottleneck_l27_28_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/bottleneck_l27_28.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/bottleneck_l27_28.c"
)

add_tpa_program(
    NAME tpa_yolo_bottleneck_l27_28
    PROCESSES
        tpa_yolo_bottleneck_l27_28_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/bottleneck_l27_28.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/bottleneck_l27_28.place"
    LINKER_SCRIPT
        "${TPA_YOLO_LDS}"
)

add_tpa_process(
    NAME tpa_yolo_c3_l40_44_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/c3_l40_44.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/c3_l40_44.c"
)

add_tpa_program(
    NAME tpa_yolo_c3_l40_44
    PROCESSES
        tpa_yolo_c3_l40_44_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/c3_l40_44.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/c3_l40_44.place"
    LINKER_SCRIPT
        "${TPA_YOLO_LDS}"
)

add_tpa_process(
    NAME tpa_yolo_sppf_l31_32_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/sppf_l31_32.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/sppf_l31_32.c"
)

add_tpa_program(
    NAME tpa_yolo_sppf_l31_32
    PROCESSES
        tpa_yolo_sppf_l31_32_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/sppf_l31_32.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/sppf_l31_32.place"
    LINKER_SCRIPT
        "${TPA_YOLO_LDS}"
)

add_tpa_process(
    NAME tpa_yolo_detect_l57_58_69_63_64_72_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/detect_l57_58_69_63_64_72.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/detect_l57_58_69_63_64_72.c"
)

add_tpa_program(
    NAME tpa_yolo_detect_l57_58_69_63_64_72
    PROCESSES
        tpa_yolo_detect_l57_58_69_63_64_72_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/detect_l57_58_69_63_64_72.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/detect_l57_58_69_63_64_72.place"
    LINKER_SCRIPT
        "${TPA_YOLO_LDS}"
)

add_tpa_process(
    NAME tpa_yolo_neck_up_l34_39_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/neck_up_l34_39.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/neck_up_l34_39.c"
)

add_tpa_program(
    NAME tpa_yolo_neck_up_l34_39
    PROCESSES
        tpa_yolo_neck_up_l34_39_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/neck_up_l34_39.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/neck_up_l34_39.place"
    LINKER_SCRIPT
        "${TPA_YOLO_LDS}"
)

add_tpa_process(
    NAME tpa_yolo_neck_down_l45_50_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/neck_down_l45_50.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/neck_down_l45_50.c"
)

add_tpa_program(
    NAME tpa_yolo_neck_down_l45_50
    PROCESSES
        tpa_yolo_neck_down_l45_50_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/neck_down_l45_50.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/neck_down_l45_50.place"
    LINKER_SCRIPT
        "${TPA_YOLO_LDS}"
)

add_tpa_process(
    NAME tpa_yolo_subgraph_p4_p5_p6_proc
    MANIFEST
        "${YOLO_SUBGRAPH_TPM}"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/yolo_subgraph_p4_p5_p6_src.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolo_subgraph_p4.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolo_subgraph_p5.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/yolo_subgraph_p6.c"
)

add_tpa_program(
    NAME tpa_yolo_subgraph_p4_p5_p6
    PROCESSES
        tpa_yolo_subgraph_p4_p5_p6_proc
    PROGRAM
        "${YOLO_SUBGRAPH_TPP}"
    PLACEMENT
        "${YOLO_SUBGRAPH_PLACE}"
    LINKER_SCRIPT
        "${TPA_YOLO_LDS}"
)

if (TPA_PLATFORM_ID STREQUAL "erbium" AND BUILD_TESTING AND EXISTS "${ERBIUM_EMU}")
    add_test(
        NAME tpa_yolo_cbs_l40
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolo_cbs_l40.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolo_cbs_l40.out
            -DMAX_CYCLES=200000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolo_bottleneck_l27_28
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolo_bottleneck_l27_28.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolo_bottleneck_l27_28.out
            -DMAX_CYCLES=200000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolo_c3_l40_44
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolo_c3_l40_44.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolo_c3_l40_44.out
            -DMAX_CYCLES=300000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolo_sppf_l31_32
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolo_sppf_l31_32.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolo_sppf_l31_32.out
            -DMAX_CYCLES=300000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolo_detect_l57_58_69_63_64_72
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolo_detect_l57_58_69_63_64_72.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolo_detect_l57_58_69_63_64_72.out
            -DMAX_CYCLES=400000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolo_neck_up_l34_39
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolo_neck_up_l34_39.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolo_neck_up_l34_39.out
            -DMAX_CYCLES=400000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolo_neck_down_l45_50
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolo_neck_down_l45_50.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolo_neck_down_l45_50.out
            -DMAX_CYCLES=400000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
    add_test(
        NAME tpa_yolo_subgraph_p4_p5_p6
        COMMAND "${CMAKE_COMMAND}"
            -DEMU=${ERBIUM_EMU}
            -DELF=$<TARGET_FILE:tpa_yolo_subgraph_p4_p5_p6.elf>
            -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_yolo_subgraph_p4_p5_p6.out
            -DMAX_CYCLES=800000000
            -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
    )
endif()
