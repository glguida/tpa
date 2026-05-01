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

set(tpa_tensor_matmul_rows 8)
set(tpa_tensor_matmul_cols 8)
set(tpa_tensor_matmul_rounds 4)
set(tpa_tensor_matmul_tile_n 64)
set(tpa_tensor_matmul_tile_bytes 16384)
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
        -DNR_MINIONS=8
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
