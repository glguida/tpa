cmake_minimum_required(VERSION 3.20)

project(tpa-host VERSION 0.1.0 LANGUAGES CXX)

if (POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

if (DEFINED ET_ROOT AND NOT "${ET_ROOT}" STREQUAL "")
    list(PREPEND CMAKE_MODULE_PATH
        "${ET_ROOT}/lib/cmake"
        "${ET_ROOT}/lib/cmake/cmake-modules"
    )
endif()
if (DEFINED ET_PLATFORM_PATH AND NOT "${ET_PLATFORM_PATH}" STREQUAL "")
    list(PREPEND CMAKE_MODULE_PATH
        "${ET_PLATFORM_PATH}/lib/cmake"
        "${ET_PLATFORM_PATH}/lib/cmake/cmake-modules"
    )
endif()
list(REMOVE_DUPLICATES CMAKE_MODULE_PATH)

set(TPA_ET_HINTS)
if (DEFINED ET_ROOT AND NOT "${ET_ROOT}" STREQUAL "")
    list(APPEND TPA_ET_HINTS "${ET_ROOT}")
endif()
if (DEFINED ET_PLATFORM_PATH AND NOT "${ET_PLATFORM_PATH}" STREQUAL "")
    list(APPEND TPA_ET_HINTS "${ET_PLATFORM_PATH}")
endif()
list(APPEND TPA_ET_HINTS ${CMAKE_MODULE_PATH})
list(APPEND TPA_ET_HINTS "/opt/et")
list(REMOVE_DUPLICATES TPA_ET_HINTS)

function(tpa_find_et_package_dir package config)
    if (DEFINED ${package}_DIR AND NOT "${${package}_DIR}" STREQUAL "")
        return()
    endif()

    string(MAKE_C_IDENTIFIER "${package}" package_id)
    string(TOUPPER "${package_id}" package_id_upper)
    find_file(TPA_${package_id_upper}_CONFIG
        NAMES "${config}"
        HINTS ${TPA_ET_HINTS}
        PATH_SUFFIXES
            "lib/cmake/${package}"
            "${package}"
        NO_DEFAULT_PATH
    )

    if (TPA_${package_id_upper}_CONFIG)
        get_filename_component(package_dir "${TPA_${package_id_upper}_CONFIG}" DIRECTORY)
        set("${package}_DIR" "${package_dir}" CACHE PATH "${package} CMake package directory" FORCE)
    endif()
endfunction()

tpa_find_et_package_dir(g3log g3logConfig.cmake)
tpa_find_et_package_dir(hostUtils hostUtilsConfig.cmake)
tpa_find_et_package_dir(sw-sysemu sw-sysemuConfig.cmake)
tpa_find_et_package_dir(deviceLayer deviceLayerConfig.cmake)
tpa_find_et_package_dir(deviceApi deviceApiConfig.cmake)
tpa_find_et_package_dir(cereal cerealConfig.cmake)
tpa_find_et_package_dir(easy_profiler easy_profilerConfig.cmake)
tpa_find_et_package_dir(runtime runtimeConfig.cmake)
tpa_find_et_package_dir(EsperantoBootLoader EsperantoBootLoaderConfig.cmake)
tpa_find_et_package_dir(EsperantoDeviceMinionRuntime EsperantoDeviceMinionRuntimeConfig.cmake)

find_package(runtime REQUIRED)
find_package(deviceLayer REQUIRED)
find_package(sw-sysemu REQUIRED)
find_package(hostUtils REQUIRED)
find_package(EsperantoBootLoader QUIET)
find_package(EsperantoDeviceMinionRuntime QUIET)

# ET's HostProject wrapper passes these before this subproject has real host
# binaries. Consume them so configure output stays focused on meaningful issues.
foreach (var IN ITEMS
    CMAKE_BUILD_RPATH
    CMAKE_BUILD_WITH_INSTALL_RPATH
    CMAKE_INSTALL_RPATH
)
    if (DEFINED ${var})
        set(tpa_host_${var} "${${var}}")
    endif()
endforeach()

function(tpa_imported_target_file out_var target)
    if (TARGET "${target}")
        get_property(target_file TARGET "${target}" PROPERTY LOCATION)
        set("${out_var}" "${target_file}" PARENT_SCOPE)
    else()
        set("${out_var}" "" PARENT_SCOPE)
    endif()
endfunction()

tpa_imported_target_file(TPA_BOOTROM_TRAMPOLINE_TO_BL2_ELF EsperantoBootLoader::BootromTrampolineToBL2.elf)
tpa_imported_target_file(TPA_BL2_ELF EsperantoBootLoader::ServiceProcessorBL2_fast-boot.elf)
tpa_imported_target_file(TPA_MACHINE_MINION_ELF EsperantoDeviceMinionRuntime::MachineMinion.elf)
tpa_imported_target_file(TPA_MASTER_MINION_ELF EsperantoDeviceMinionRuntime::MasterMinion.elf)
tpa_imported_target_file(TPA_WORKER_MINION_ELF EsperantoDeviceMinionRuntime::WorkerMinion.elf)

if (DEFINED runtime_DIR)
    get_filename_component(TPA_ET_PREFIX "${runtime_DIR}/../../.." ABSOLUTE)
elseif (DEFINED ET_ROOT AND NOT "${ET_ROOT}" STREQUAL "")
    set(TPA_ET_PREFIX "${ET_ROOT}")
elseif (DEFINED ET_PLATFORM_PATH AND NOT "${ET_PLATFORM_PATH}" STREQUAL "")
    set(TPA_ET_PREFIX "${ET_PLATFORM_PATH}")
endif()

find_program(TPA_SYSEMU_EXECUTABLE
    NAMES sys_emu
    HINTS
        "${TPA_ET_PREFIX}/bin"
        "${CMAKE_INSTALL_PREFIX}/bin"
    PATH_SUFFIXES bin
)

add_executable(tpa_launcher
    src/tpa_launcher.cpp
)

target_link_libraries(tpa_launcher
    PRIVATE
        runtime::etrt
        deviceLayer::deviceLayer
        sw-sysemu::sw-sysemu
        hostUtils::logging
)

target_compile_definitions(tpa_launcher
    PRIVATE
        TPA_SYSEMU_EXECUTABLE="${TPA_SYSEMU_EXECUTABLE}"
        TPA_BOOTROM_TRAMPOLINE_TO_BL2_ELF="${TPA_BOOTROM_TRAMPOLINE_TO_BL2_ELF}"
        TPA_BL2_ELF="${TPA_BL2_ELF}"
        TPA_MACHINE_MINION_ELF="${TPA_MACHINE_MINION_ELF}"
        TPA_MASTER_MINION_ELF="${TPA_MASTER_MINION_ELF}"
        TPA_WORKER_MINION_ELF="${TPA_WORKER_MINION_ELF}"
)

add_custom_target(tpa_host_tools DEPENDS tpa_launcher)
