set(TPA_LTFARM_LDS "${TPA_PLATFORM_LINKER_SCRIPT}")
set(TPA_LTFARM_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

get_filename_component(TPA_ERBIUM_EMU_BUILD_DIR "${ERBIUM_EMU}" DIRECTORY)
get_filename_component(TPA_ERBIUM_EMU_SOURCE_DIR "${TPA_ERBIUM_EMU_BUILD_DIR}" DIRECTORY)
set(TPA_ERBIUM_EMU_LIB "${TPA_ERBIUM_EMU_BUILD_DIR}/libsw-erbium.a")
set(TPA_ERBIUM_EMU_BUILD_INCLUDE "${TPA_ERBIUM_EMU_BUILD_DIR}/sw-sysemu/include/sw-sysemu")
set(TPA_ERBIUM_EMU_EXPORT_INCLUDE "${TPA_ERBIUM_EMU_SOURCE_DIR}/sw-sysemu/include")
set(TPA_ERBIUM_EMU_ROOT_INCLUDE "${TPA_ERBIUM_EMU_SOURCE_DIR}")
set(TPA_ERBIUM_EMU_SYS_INCLUDE "${TPA_ERBIUM_EMU_SOURCE_DIR}/sys_emu")

add_tpa_process(
    NAME tpa_ltfarm_job_source_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_job_source.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_job_source.c"
    INCLUDES
        ${TPA_LTFARM_INCLUDES}
)

add_tpa_process(
    NAME tpa_ltfarm_scrypt_worker_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_scrypt_core.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_scrypt_core.c"
    INCLUDES
        ${TPA_LTFARM_INCLUDES}
)

add_tpa_process(
    NAME tpa_ltfarm_result_sink_proc
    MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_result_sink.tpm"
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_result_sink.c"
    INCLUDES
        ${TPA_LTFARM_INCLUDES}
)

add_tpa_program(
    NAME tpa_ltfarm_scrypt_core
    PROCESSES
        tpa_ltfarm_job_source_proc
        tpa_ltfarm_scrypt_worker_proc
        tpa_ltfarm_result_sink_proc
    PROGRAM
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_scrypt_core.tpp"
    PLACEMENT
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_scrypt_core.place"
    LINKER_SCRIPT
        "${TPA_LTFARM_LDS}"
    INCLUDES
        ${TPA_LTFARM_INCLUDES}
)

set(TPA_LTFARM_SCRYPT_HOST_BIN "${CMAKE_CURRENT_BINARY_DIR}/ltfarm_scrypt_core_host")
add_custom_command(
    OUTPUT "${TPA_LTFARM_SCRYPT_HOST_BIN}"
    COMMAND /usr/bin/c++
        -std=c++17
        -g
        -DERBIUM
        -DSYS_EMU
        -I${CMAKE_CURRENT_SOURCE_DIR}
        -I${TPA_ERBIUM_EMU_ROOT_INCLUDE}
        -I${TPA_ERBIUM_EMU_SYS_INCLUDE}
        -I${TPA_ERBIUM_EMU_BUILD_INCLUDE}
        -I${TPA_ERBIUM_EMU_EXPORT_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_scrypt_core_host.cpp
        -o ${TPA_LTFARM_SCRYPT_HOST_BIN}
        ${TPA_ERBIUM_EMU_LIB}
        /usr/lib/x86_64-linux-gnu/liblz4.a
        /usr/lib/x86_64-linux-gnu/libglog.so
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_scrypt_core_host.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_worker_core.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/ltfarm_litecoin_core_vectors.h"
        tpa_ltfarm_scrypt_core.elf
        "${TPA_ERBIUM_EMU_LIB}"
    VERBATIM
)

add_custom_target(ltfarm_scrypt_core_host DEPENDS "${TPA_LTFARM_SCRYPT_HOST_BIN}")

add_custom_target(
    ltfarm_scrypt_core_check
    COMMAND "${TPA_LTFARM_SCRYPT_HOST_BIN}" --elf "$<TARGET_FILE:tpa_ltfarm_scrypt_core.elf>" --all
    DEPENDS
        ltfarm_scrypt_core_host
        tpa_ltfarm_scrypt_core.elf
    VERBATIM
)

add_custom_target(
    ltfarm_scrypt_core_packed_check
    COMMAND "${TPA_LTFARM_SCRYPT_HOST_BIN}" --elf "$<TARGET_FILE:tpa_ltfarm_scrypt_core.elf>" --packed
    DEPENDS
        ltfarm_scrypt_core_host
        tpa_ltfarm_scrypt_core.elf
    VERBATIM
)
