set(TPA_DEPTH_DEMO_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")

add_tpa_process(
    NAME tpa_stereo_sad_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/stereo_sad.c"
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
