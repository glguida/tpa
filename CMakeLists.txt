cmake_minimum_required(VERSION 3.16)

project(tpa_structured LANGUAGES C)

option(TPA_BUILD_ERBIUM "Build the Erbium TPA library variant" ON)
option(TPA_BUILD_ETSOC1 "Build the ET-SoC-1 TPA library variant" ON)

include(CTest)

add_subdirectory(tpa)
