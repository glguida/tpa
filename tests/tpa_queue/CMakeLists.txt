set(TPA_QUEUE_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")

add_tpa_process(
    NAME tpa_queue_basic_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/basic.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/basic.c"
)

add_tpa_program_test(
    NAME tpa_queue_basic
    PROCESSES
        tpa_queue_basic_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/basic.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/basic.place"
    LINKER_SCRIPT
        "${TPA_QUEUE_LDS}"
)

add_tpa_process(
    NAME tpa_queue_yield_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/yield.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/yield.c"
)

add_tpa_program_test(
    NAME tpa_queue_yield
    PROCESSES
        tpa_queue_yield_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/yield.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/yield.place"
    LINKER_SCRIPT
        "${TPA_QUEUE_LDS}"
)

add_tpa_process(
    NAME tpa_queue_many_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/many.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/many.c"
)

add_tpa_program_test(
    NAME tpa_queue_many
    PROCESSES
        tpa_queue_many_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/many.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/many.place"
    LINKER_SCRIPT
        "${TPA_QUEUE_LDS}"
)

add_tpa_process(
    NAME tpa_queue_wake_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/wake.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/wake.c"
)

add_tpa_program_test(
    NAME tpa_queue_wake
    PROCESSES
        tpa_queue_wake_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/wake.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/wake.place"
    LINKER_SCRIPT
        "${TPA_QUEUE_LDS}"
)
