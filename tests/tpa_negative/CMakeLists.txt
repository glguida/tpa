set(TPA_NEGATIVE_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")

# Deliberately not add_tpa_program_test(): this ELF is meant to fail when run.
add_tpa_process(
    NAME tpa_negative_expected_fail_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/expected_fail.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/expected_fail.c"
)

add_tpa_program(
    NAME tpa_negative_expected_fail
    PROCESSES
        tpa_negative_expected_fail_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/expected_fail.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/expected_fail.place"
    LINKER_SCRIPT
        "${TPA_NEGATIVE_LDS}"
)
