add_subdirectory(tpa_queue)
add_subdirectory(tpa_msg)
add_subdirectory(tpa_negative)

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

        function(tpa_add_erbium_runtime_test test_name elf_target)
            add_test(
                NAME ${test_name}
                COMMAND "${CMAKE_COMMAND}"
                    -DEMU=${TPA_ERBIUM_EMU}
                    -DELF=$<TARGET_FILE:${elf_target}.elf>
                    -DLOG=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.log
                    -P "${TPA_ROOT_DIR}/cmake/run_erbium_test_fast.cmake"
            )
        endfunction()

        function(tpa_add_erbium_expected_fail_test test_name elf_target)
            add_test(
                NAME ${test_name}
                COMMAND "${CMAKE_COMMAND}"
                    -DEMU=${TPA_ERBIUM_EMU}
                    -DELF=$<TARGET_FILE:${elf_target}.elf>
                    -DLOG=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.log
                    -P "${TPA_ROOT_DIR}/cmake/run_erbium_expected_fail.cmake"
            )
        endfunction()

        tpa_add_erbium_runtime_test(tpa_queue_basic_erbium tpa_queue_basic)
        tpa_add_erbium_runtime_test(tpa_queue_yield_erbium tpa_queue_yield)
        tpa_add_erbium_runtime_test(tpa_msg_same_send_first_erbium tpa_msg_same_send_first)
        tpa_add_erbium_runtime_test(tpa_msg_cross_send_first_erbium tpa_msg_cross_send_first)
    endif()
endif()
