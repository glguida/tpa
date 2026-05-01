set(TPA_MSG_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")

add_tpa_process(
    NAME tpa_msg_cross_send_first_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_send_first.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_send_first.c"
)

add_tpa_program_test(
    NAME tpa_msg_cross_send_first
    PROCESSES
        tpa_msg_cross_send_first_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_send_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_send_first.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
)

add_tpa_program_test(
    NAME tpa_msg_fabric_send_first
    PROCESSES
        tpa_msg_cross_send_first_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_send_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_send_first_fabric.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
)

add_tpa_process(
    NAME tpa_msg_cross_recv_first_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_recv_first.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_recv_first.c"
)

add_tpa_program_test(
    NAME tpa_msg_cross_recv_first
    PROCESSES
        tpa_msg_cross_recv_first_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_recv_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_recv_first.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
)

add_tpa_program_test(
    NAME tpa_msg_fabric_recv_first
    PROCESSES
        tpa_msg_cross_recv_first_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_recv_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/cross_recv_first_fabric.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
)

add_tpa_process(
    NAME tpa_msg_same_send_first_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/same_send_first.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/same_send_first.c"
)

add_tpa_program_test(
    NAME tpa_msg_same_send_first
    PROCESSES
        tpa_msg_same_send_first_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/same_send_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/same_send_first.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
)

add_tpa_process(
    NAME tpa_msg_same_recv_first_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/same_recv_first.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/same_recv_first.c"
)

add_tpa_program_test(
    NAME tpa_msg_same_recv_first
    PROCESSES
        tpa_msg_same_recv_first_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/same_recv_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/same_recv_first.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
)

add_tpa_process(
    NAME tpa_msg_bench_100_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/bench_100.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/bench_100.c"
)

add_tpa_program_test(
    NAME tpa_msg_bench_100
    PROCESSES
        tpa_msg_bench_100_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/bench_100.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/bench_100.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
)

add_tpa_process(
    NAME tpa_msg_race_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/race.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/race.c"
)

add_tpa_program_test(
    NAME tpa_msg_fabric_race
    PROCESSES
        tpa_msg_race_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/race.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/race_fabric.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
)

add_tpa_process(
    NAME tpa_msg_edge_storage_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage.c"
)

add_tpa_program_test(
    NAME tpa_msg_edge_storage_direct_send_first
    PROCESSES
        tpa_msg_edge_storage_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_send_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_send_first_direct.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
    EDGE_CONFIG_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_config.h"
)

add_tpa_program_test(
    NAME tpa_msg_edge_storage_direct_recv_first
    PROCESSES
        tpa_msg_edge_storage_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_recv_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_recv_first_direct.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
    EDGE_CONFIG_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_config.h"
)

add_tpa_program_test(
    NAME tpa_msg_edge_storage_local_send_first
    PROCESSES
        tpa_msg_edge_storage_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_send_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_send_first_local.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
    EDGE_CONFIG_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_config.h"
)

add_tpa_program_test(
    NAME tpa_msg_edge_storage_local_recv_first
    PROCESSES
        tpa_msg_edge_storage_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_recv_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_recv_first_local.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
    EDGE_CONFIG_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_config.h"
)

add_tpa_program_test(
    NAME tpa_msg_edge_storage_fabric_send_first
    PROCESSES
        tpa_msg_edge_storage_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_send_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_send_first_fabric.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
    EDGE_CONFIG_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_config.h"
)

add_tpa_program_test(
    NAME tpa_msg_edge_storage_fabric_recv_first
    PROCESSES
        tpa_msg_edge_storage_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_recv_first.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_recv_first_fabric.place"
    LINKER_SCRIPT
        "${TPA_MSG_LDS}"
    EDGE_CONFIG_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/edge_storage_config.h"
)
