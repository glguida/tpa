add_tpa_process(
    NAME tpa_attention_qkv_gen_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_qkv_gen.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_qkv_gen.c"
)

add_tpa_process(
    NAME tpa_attention_score_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_score.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_score.c"
)

add_tpa_process(
    NAME tpa_attention_softmax_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_softmax.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_softmax.c"
)

add_tpa_process(
    NAME tpa_attention_output_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_output.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_output.c"
)

set(tpa_attention_processes
    tpa_attention_qkv_gen_proc
    tpa_attention_score_proc
    tpa_attention_softmax_proc
    tpa_attention_output_proc
)

add_tpa_program(
    NAME tpa_fast_attention
    PROCESSES
        ${tpa_attention_processes}
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/attention.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/attention.place"
)

add_tpa_program(
    NAME tpa_fast_attention_serial
    PROCESSES
        ${tpa_attention_processes}
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/attention.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/attention_serial.place"
)
