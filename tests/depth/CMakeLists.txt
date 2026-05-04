set(TPA_DEPTH_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")

add_tpa_process(
    NAME tpa_depth_sad_block_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/depth_sad_block.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/depth_sad_block.c"
)

add_tpa_program_test(
    NAME tpa_depth_sad_cost_5x5
    PROCESSES
        tpa_depth_sad_block_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/cost_5x5.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/cost_5x5.place"
    LINKER_SCRIPT
        "${TPA_DEPTH_LDS}"
)

add_tpa_program_test(
    NAME tpa_depth_sad_argmin_16x8
    PROCESSES
        tpa_depth_sad_block_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/argmin_16x8.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/argmin_16x8.place"
    LINKER_SCRIPT
        "${TPA_DEPTH_LDS}"
)

add_tpa_program_test(
    NAME tpa_depth_sad_stripe_96x16
    PROCESSES
        tpa_depth_sad_block_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/stripe_96x16.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/stripe_96x16.place"
    LINKER_SCRIPT
        "${TPA_DEPTH_LDS}"
)
