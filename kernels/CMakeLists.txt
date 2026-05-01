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
