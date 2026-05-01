set(TPA_CORE_SOURCES
    lib/src/channel.c
    lib/src/process.c
    lib/src/scheduler.c
)

function(tpa_add_platform_library target hal_source)
    add_library(${target} STATIC ${TPA_CORE_SOURCES} ${hal_source})
    add_library(tpa::${target} ALIAS ${target})

    target_include_directories(${target}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/lib/include
            ${CMAKE_CURRENT_SOURCE_DIR}/hal/include
    )

    target_compile_features(${target} PUBLIC c_std_11)

    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Werror)
    endif()
endfunction()

if(TPA_BUILD_ERBIUM)
    tpa_add_platform_library(tpa_erbium hal/erbium/hal_erbium.c)
    target_compile_definitions(tpa_erbium
        PUBLIC
            TPA_HAL_NR_HARTS=16
            TPA_HAL_CACHELINE_BYTES=64u
            TPA_HAL_L1D_BYTES=4096u
    )
endif()

if(TPA_BUILD_ETSOC1)
    tpa_add_platform_library(tpa_etsoc1 hal/etsoc1/hal_etsoc1.c)
    target_compile_definitions(tpa_etsoc1
        PUBLIC
            TPA_ETSOC1_NR_SHIRES=1
            TPA_ETSOC1_MINIONS_PER_SHIRE=16u
            TPA_ETSOC1_HARTS_PER_MINION=2u
            TPA_HAL_NR_HARTS=32
            TPA_HAL_CACHELINE_BYTES=64u
            TPA_HAL_L1D_BYTES=4096u
    )
endif()
