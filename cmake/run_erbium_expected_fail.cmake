if (NOT DEFINED EMU)
    message(FATAL_ERROR "run_erbium_expected_fail.cmake needs EMU")
endif()

if (NOT DEFINED ELF)
    message(FATAL_ERROR "run_erbium_expected_fail.cmake needs ELF")
endif()

if (NOT DEFINED LOG)
    message(FATAL_ERROR "run_erbium_expected_fail.cmake needs LOG")
endif()

if (NOT DEFINED MAX_CYCLES)
    set(MAX_CYCLES 100000000)
endif()

execute_process(
    COMMAND "${EMU}" -minions 0xff -mins_dis -max_cycles "${MAX_CYCLES}"
            -elf_load "${ELF}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)

file(WRITE "${LOG}" "${out}${err}")

if (out MATCHES "Signal end test with FAIL" OR
    err MATCHES "Signal end test with FAIL")
    return()
endif()

if (out MATCHES "Signal end test with PASS" OR
    err MATCHES "Signal end test with PASS")
    message(FATAL_ERROR "negative test unexpectedly reported PASS\nsee ${LOG}")
endif()

if (NOT rc EQUAL 0)
    return()
endif()

message(FATAL_ERROR "negative test did not report expected FAIL\nsee ${LOG}")
