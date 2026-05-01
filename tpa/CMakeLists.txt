set(TPA_CORE_SOURCES
    lib/src/channel.c
    lib/src/process.c
    lib/src/scheduler.c
)

function(tpa_add_platform_library target hal_source platform_include_dir platform_header)
    add_library(${target} STATIC ${TPA_CORE_SOURCES} ${hal_source})
    add_library(tpa::${target} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
            ${CMAKE_CURRENT_SOURCE_DIR}/${platform_include_dir}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/hal/include
    )

    target_compile_features(${target} PUBLIC c_std_11)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target}
            PUBLIC
                -include ${platform_header}
            PRIVATE
                -Wall -Wextra -Werror
        )
    endif()
endfunction()

if(TPA_BUILD_ERBIUM)
    tpa_add_platform_library(
        tpa_erbium
        hal/erbium/hal_erbium.c
        hal/erbium
        tpa/hal/erbium.h
    )
endif()

if(TPA_BUILD_ETSOC1)
    tpa_add_platform_library(
        tpa_etsoc1
        hal/etsoc1/hal_etsoc1.c
        hal/etsoc1
        tpa/hal/etsoc1.h
    )
endif()

if(BUILD_TESTING)
    foreach(platform IN ITEMS tpa_erbium tpa_etsoc1)
        if(TARGET ${platform})
            add_executable(${platform}_smoke tests/smoke.c)
            target_link_libraries(${platform}_smoke PRIVATE ${platform})
            if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
                target_compile_options(${platform}_smoke PRIVATE -Wall -Wextra -Werror)
            endif()
            add_test(NAME ${platform}_smoke COMMAND ${platform}_smoke)
        endif()
    endforeach()
endif()
