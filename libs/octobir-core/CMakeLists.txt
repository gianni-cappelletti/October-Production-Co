cmake_minimum_required(VERSION 3.15)
project(octobir-core VERSION 2.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/IRLoader.cpp
    src/IRProcessor.cpp
)

add_library(octobir-core STATIC ${SOURCES})

target_include_directories(octobir-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Mark third-party includes as SYSTEM to suppress warnings from external code
target_include_directories(octobir-core SYSTEM
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/WDL/WDL
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party
)

set(WDL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/WDL/WDL)

set(WDL_SOURCES
    ${WDL_DIR}/convoengine.cpp
    ${WDL_DIR}/fft.c
    ${WDL_DIR}/resample.cpp
)

target_sources(octobir-core PRIVATE ${WDL_SOURCES})

set(PFFFT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/pffft)

set(PFFFT_SOURCES
    ${PFFFT_DIR}/src/pffft.c
    ${PFFFT_DIR}/src/pffft_common.c
)

target_sources(octobir-core PRIVATE ${PFFFT_SOURCES})

target_include_directories(octobir-core SYSTEM
    PRIVATE
        ${PFFFT_DIR}/include/pffft
)

# Enable NEON SIMD for PFFFT on ARM (without this, PFFFT falls back to scalar code)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set_source_files_properties(${PFFFT_SOURCES}
        PROPERTIES
        COMPILE_DEFINITIONS "PFFFT_ENABLE_NEON=1"
    )
endif()

# Suppress warnings in third-party code that we cannot modify
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set_source_files_properties(${WDL_SOURCES} ${PFFFT_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "-Wno-unused-parameter -ffast-math"
    )
endif()

if(APPLE)
    target_compile_options(octobir-core PRIVATE -Wall -Wextra)
elseif(WIN32)
    target_compile_definitions(octobir-core PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
    )
endif()

set_target_properties(octobir-core PROPERTIES
    PUBLIC_HEADER "include/octobir-core/IRProcessor.hpp;include/octobir-core/IRLoader.hpp;include/octobir-core/Types.hpp"
    POSITION_INDEPENDENT_CODE ON
)

option(BUILD_OCTOBIR_CORE_TESTS "Build octobir-core unit tests" OFF)
if(BUILD_OCTOBIR_CORE_TESTS)
    add_subdirectory(tests)
endif()
