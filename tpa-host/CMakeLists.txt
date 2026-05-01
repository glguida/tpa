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
tpa_find_et_package_dir(runtime runtimeConfig.cmake)

find_package(runtime REQUIRED)
find_package(deviceLayer REQUIRED)
find_package(sw-sysemu REQUIRED)

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

# The structured host launcher/tooling has not been fully ported yet. Keep this
# real ET host subproject in the superbuild so package discovery and wrapper
# integration are validated, and provide a named target for follow-up work.
add_custom_target(tpa_host_tools
    COMMAND "${CMAKE_COMMAND}" -E echo
        "TPA structured host launcher is not yet ported; ET host packages configured successfully."
    VERBATIM
)
