cmake_minimum_required(VERSION 3.20)

if (NOT DEFINED TPA_ROOT_DIR)
    get_filename_component(TPA_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

if (DEFINED ET_ROOT AND NOT "${ET_ROOT}" STREQUAL "")
    list(APPEND TPA_ET_ROOTS "${ET_ROOT}")
endif()
if (DEFINED ET_PLATFORM_PATH AND NOT "${ET_PLATFORM_PATH}" STREQUAL "")
    list(APPEND TPA_ET_ROOTS "${ET_PLATFORM_PATH}")
endif()
list(APPEND TPA_ET_ROOTS ${CMAKE_MODULE_PATH})
list(APPEND TPA_ET_ROOTS "/opt/et")
list(REMOVE_DUPLICATES TPA_ET_ROOTS)

set(TPA_ET_CMAKE_PATHS)
foreach (root IN LISTS TPA_ET_ROOTS)
    list(APPEND TPA_ET_CMAKE_PATHS
        "${root}"
        "${root}/lib/cmake/aifoundry-utils"
        "${root}/lib/cmake"
        "${root}/lib/cmake/cmake-modules"
    )
endforeach()
list(REMOVE_DUPLICATES TPA_ET_CMAKE_PATHS)

if (NOT CMAKE_TOOLCHAIN_FILE)
    unset(ET_RISCV64_EC_TOOLCHAIN CACHE)
    find_file(ET_RISCV64_EC_TOOLCHAIN
        NAMES riscv64-ec-toolchain.cmake
        HINTS ${TPA_ET_CMAKE_PATHS}
        PATH_SUFFIXES lib/cmake
        NO_DEFAULT_PATH
    )

    if (NOT ET_RISCV64_EC_TOOLCHAIN)
        message(FATAL_ERROR
            "Could not find riscv64-ec-toolchain.cmake. Set ET_ROOT or ET_PLATFORM_PATH "
            "to an et-platform install; host smoke tests are separate and use "
            "-DTPA_HOST_SMOKE_TEST_DOUBLE=ON at the top level.")
    endif()

    set(CMAKE_TOOLCHAIN_FILE "${ET_RISCV64_EC_TOOLCHAIN}"
        CACHE FILEPATH "ET RISC-V device toolchain")
endif()

if (NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "TPA device build type" FORCE)
endif()

project(tpa-device VERSION 0.1.0 LANGUAGES C ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(GNUInstallDirs)

list(APPEND CMAKE_PREFIX_PATH ${TPA_ET_ROOTS})
list(APPEND CMAKE_MODULE_PATH
    ${TPA_ET_CMAKE_PATHS}
    "${TPA_ROOT_DIR}/cmake"
)
list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)

if (NOT CMAKE_C_COMPILER MATCHES "riscv64-unknown-elf")
    message(FATAL_ERROR
        "Configure tpa-device with the ET RISC-V device toolchain. "
        "CMAKE_C_COMPILER='${CMAKE_C_COMPILER}' is not an ET riscv64-unknown-elf compiler.")
endif()

include(DeviceUtils)
include(tpa-kernel)
find_package(et-common-libs REQUIRED COMPONENTS cm-umode)

set(TPA_PLATFORM "erbium" CACHE STRING "TPA device platform")
set_property(CACHE TPA_PLATFORM PROPERTY STRINGS erbium etsoc1)
string(TOLOWER "${TPA_PLATFORM}" TPA_PLATFORM_ID)
set(TPA_ETSOC1_NR_SHIRES 1 CACHE STRING "Number of ET-SoC-1 shires with compiled TPA queues")

set(TPA_CORE_SOURCES
    "${TPA_ROOT_DIR}/tpa/lib/src/channel.c"
    "${TPA_ROOT_DIR}/tpa/lib/src/process.c"
    "${TPA_ROOT_DIR}/tpa/lib/src/scheduler.c"
)

if (TPA_PLATFORM_ID STREQUAL "erbium")
    set(TPA_PLATFORM_HEADER tpa/hal/erbium.h)
    set(TPA_PLATFORM_HAL_SOURCE "${TPA_ROOT_DIR}/tpa/hal/erbium/hal_erbium.c")
    set(TPA_PLATFORM_INCLUDE_DIR "${TPA_ROOT_DIR}/tpa/hal/erbium/include")
elseif (TPA_PLATFORM_ID STREQUAL "etsoc1")
    set(TPA_PLATFORM_HEADER tpa/hal/etsoc1.h)
    set(TPA_PLATFORM_HAL_SOURCE "${TPA_ROOT_DIR}/tpa/hal/etsoc1/hal_etsoc1.c")
    set(TPA_PLATFORM_INCLUDE_DIR "${TPA_ROOT_DIR}/tpa/hal/etsoc1/include")
else()
    message(FATAL_ERROR "Unsupported TPA_PLATFORM='${TPA_PLATFORM}'. Expected erbium or etsoc1.")
endif()

add_library(tpa_core STATIC
    ${TPA_CORE_SOURCES}
    "${TPA_PLATFORM_HAL_SOURCE}"
)
add_library(tpa::core ALIAS tpa_core)

target_include_directories(tpa_core
    PUBLIC
        "${TPA_ROOT_DIR}/tpa/lib/include"
        "${TPA_ROOT_DIR}/tpa/hal/include"
        "${TPA_PLATFORM_INCLUDE_DIR}"
)

target_compile_features(tpa_core PUBLIC c_std_11)
target_compile_options(tpa_core PRIVATE -Wall -Wextra -Werror)
target_compile_definitions(tpa_core PUBLIC TPA_DEVICE_BUILD=1 TPA_ETSOC1_NR_SHIRES=${TPA_ETSOC1_NR_SHIRES})
target_compile_options(tpa_core PUBLIC $<$<COMPILE_LANGUAGE:C>:-include> $<$<COMPILE_LANGUAGE:C>:${TPA_PLATFORM_HEADER}>)
target_link_libraries(tpa_core PUBLIC et-common-libs::cm-umode)

option(BUILD_TPA_KERNELS "Build structured TPA kernel/demo programs" ON)

if (BUILD_TPA_KERNELS)
    add_subdirectory("${TPA_ROOT_DIR}/kernels" "${CMAKE_BINARY_DIR}/kernels")
endif()
