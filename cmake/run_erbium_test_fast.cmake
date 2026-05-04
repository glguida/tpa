if (NOT DEFINED EMU)
    message(FATAL_ERROR "run_erbium_test_fast.cmake needs EMU")
endif()

if (NOT DEFINED ELF)
    message(FATAL_ERROR "run_erbium_test_fast.cmake needs ELF")
endif()

if (NOT DEFINED LOG)
    message(FATAL_ERROR "run_erbium_test_fast.cmake needs LOG")
endif()

if (NOT DEFINED MAX_CYCLES)
    set(MAX_CYCLES 100000000)
endif()

set(EMU_ARGS)
if (DEFINED MINIONS AND NOT "${MINIONS}" STREQUAL "")
    list(APPEND EMU_ARGS -minions "${MINIONS}")
endif()

execute_process(
    COMMAND "${EMU}" ${EMU_ARGS} -max_cycles "${MAX_CYCLES}" -elf_load "${ELF}"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
)

file(WRITE "${LOG}" "${out}${err}")

if (out MATCHES "Signal end test with FAIL" OR
    err MATCHES "Signal end test with FAIL")
    message(FATAL_ERROR "test reported FAIL\nsee ${LOG}")
endif()

if (out MATCHES "Signal end test with PASS" OR
    err MATCHES "Signal end test with PASS")
    return()
endif()

if (NOT rc EQUAL 0)
    message(FATAL_ERROR
        "emulator failed with rc=${rc}\n"
        "see ${LOG}")
endif()

message(FATAL_ERROR "test did not report PASS\nsee ${LOG}")
