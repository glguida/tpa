add_tpa_process(
    NAME tpa_empty_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_empty.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_empty.c"
)

add_tpa_program(
    NAME tpa_empty
    PROCESSES
        tpa_empty_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_empty.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_empty.place"
)

add_tpa_process(
    NAME tpa_pipe_demo_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pipe_demo.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pipe_demo.c"
)

add_tpa_program(
    NAME tpa_pipe_demo
    PROCESSES
        tpa_pipe_demo_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pipe_demo.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pipe_demo.place"
)

add_tpa_process(
    NAME tpa_packed_single_row_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_packed_single_row.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_packed_single_row.c"
)

add_tpa_program(
    NAME tpa_packed_single_row
    PROCESSES
        tpa_packed_single_row_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_packed_single_row.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_packed_single_row.place"
)

add_tpa_process(
    NAME tpa_tensor_alignment_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_tensor_alignment.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_tensor_alignment.c"
)

add_tpa_program(
    NAME tpa_tensor_alignment
    PROCESSES
        tpa_tensor_alignment_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_tensor_alignment.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_tensor_alignment.place"
)

add_tpa_process(
    NAME tpa_pmu_counter_sanity_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pmu_counter_sanity.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pmu_counter_sanity.c"
)

add_tpa_program(
    NAME tpa_pmu_counter_sanity
    PROCESSES
        tpa_pmu_counter_sanity_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pmu_counter_sanity.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_pmu_counter_sanity.place"
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
            NAME tpa_packed_single_row
            COMMAND "${CMAKE_COMMAND}"
                -DEMU=${TPA_ERBIUM_EMU}
                -DELF=$<TARGET_FILE:tpa_packed_single_row.elf>
                -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_packed_single_row.log
                -DMAX_CYCLES=1000000
                -DMINIONS=0x7
                -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
        )
        add_test(
            NAME tpa_tensor_alignment
            COMMAND "${CMAKE_COMMAND}"
                -DEMU=${TPA_ERBIUM_EMU}
                -DELF=$<TARGET_FILE:tpa_tensor_alignment.elf>
                -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_tensor_alignment.log
                -DMAX_CYCLES=1000000
                -DMINIONS=0x3
                -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
        )
        add_test(
            NAME tpa_pmu_counter_sanity
            COMMAND "${CMAKE_COMMAND}"
                -DEMU=${TPA_ERBIUM_EMU}
                -DELF=$<TARGET_FILE:tpa_pmu_counter_sanity.elf>
                -DLOG=${CMAKE_CURRENT_BINARY_DIR}/tpa_pmu_counter_sanity.log
                -DMAX_CYCLES=100000
                -DMINIONS=0x1
                -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
        )
    endif()
endif()

set(tpa_tensor_matmul_rows 8)
set(tpa_tensor_matmul_cols 8)
set(tpa_tensor_matmul_rounds 4)
set(tpa_tensor_matmul_tile_n 64)
set(tpa_tensor_matmul_tile_bytes 16384)
# Keep the Erbium correctness demo inside the documented emulator validation
# mask (-minions 0x1f), which starts minions 0..4. Other platforms keep the
# original eight-minion hand placement.
if (TPA_PLATFORM_ID STREQUAL "erbium")
    set(tpa_tensor_matmul_nr_minions 5)
else()
    set(tpa_tensor_matmul_nr_minions 8)
endif()
set(tpa_tensor_matmul_tpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/tpa_tensor_matmul.tpp")
set(tpa_tensor_matmul_place
    "${CMAKE_CURRENT_BINARY_DIR}/generated/tpa_tensor_matmul.place")

add_custom_command(
    OUTPUT
        "${tpa_tensor_matmul_tpp}"
        "${tpa_tensor_matmul_place}"
    COMMAND "${CMAKE_COMMAND}"
        -DOUTPUT_PROGRAM=${tpa_tensor_matmul_tpp}
        -DOUTPUT_PLACEMENT=${tpa_tensor_matmul_place}
        -DROWS=${tpa_tensor_matmul_rows}
        -DCOLS=${tpa_tensor_matmul_cols}
        -DNR_MINIONS=${tpa_tensor_matmul_nr_minions}
        -DTILE_BYTES=${tpa_tensor_matmul_tile_bytes}
        -P "${CMAKE_CURRENT_SOURCE_DIR}/gen_tpa_tensor_matmul.cmake"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/gen_tpa_tensor_matmul.cmake"
    VERBATIM
)

add_tpa_process(
    NAME tpa_tensor_matmul_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_tensor_matmul.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tpa_tensor_matmul.c"
)
target_compile_definitions(tpa_tensor_matmul_proc PRIVATE
    TPA_TENSOR_MATMUL_ROWS=${tpa_tensor_matmul_rows}
    TPA_TENSOR_MATMUL_COLS=${tpa_tensor_matmul_cols}
    TPA_TENSOR_MATMUL_ROUNDS=${tpa_tensor_matmul_rounds}
    TPA_TENSOR_MATMUL_TILE_N=${tpa_tensor_matmul_tile_n}
)

add_tpa_program(
    NAME tpa_tensor_matmul
    PROCESSES
        tpa_tensor_matmul_proc
    PROGRAM
        "${tpa_tensor_matmul_tpp}"
    PLACEMENT
        "${tpa_tensor_matmul_place}"
)
