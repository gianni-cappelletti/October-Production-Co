cmake_minimum_required(VERSION 3.15)
project(octobass-core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
    src/BassProcessor.cpp
    src/BusCompressor.cpp
    src/Compressor.cpp
    src/Crossover.cpp
    src/FETCompressor.cpp
    src/GraphicEQ.cpp
    src/NamProcessor.cpp
    src/NoiseGate.cpp
    src/OptoCompressor.cpp
    src/VCACompressor.cpp
)

# NeuralAmpModelerCore sources
set(NAM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/NeuralAmpModelerCore)

set(NAM_SOURCES
    ${NAM_DIR}/NAM/activations.cpp
    ${NAM_DIR}/NAM/container.cpp
    ${NAM_DIR}/NAM/conv1d.cpp
    ${NAM_DIR}/NAM/convnet.cpp
    ${NAM_DIR}/NAM/dsp.cpp
    ${NAM_DIR}/NAM/get_dsp.cpp
    ${NAM_DIR}/NAM/lstm.cpp
    ${NAM_DIR}/NAM/ring_buffer.cpp
    ${NAM_DIR}/NAM/slimmable_wavenet.cpp
    ${NAM_DIR}/NAM/util.cpp
    ${NAM_DIR}/NAM/wavenet.cpp
)

add_library(octobass-core STATIC ${SOURCES} ${NAM_SOURCES})

target_include_directories(octobass-core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# NAM and its dependencies (Eigen, nlohmann) as SYSTEM to suppress warnings
target_include_directories(octobass-core SYSTEM
    PRIVATE
        ${NAM_DIR}
        ${NAM_DIR}/Dependencies/eigen
        ${NAM_DIR}/Dependencies/nlohmann
)

# Use float for NAM samples to match our audio pipeline
target_compile_definitions(octobass-core
    PRIVATE
        NAM_SAMPLE_FLOAT
        NAM_USE_INLINE_GEMM
)

target_link_libraries(octobass-core
    PUBLIC
        octobir-core
)

# Suppress warnings in NAM third-party code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    set_source_files_properties(${NAM_SOURCES}
        PROPERTIES
        COMPILE_FLAGS "-Wno-unused-parameter -Wno-error -ffast-math"
    )
endif()

if(APPLE)
    target_compile_options(octobass-core PRIVATE -Wall -Wextra)
elseif(WIN32)
    target_compile_definitions(octobass-core PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX
        __restrict__=__restrict
    )
endif()

set_target_properties(octobass-core PROPERTIES
    PUBLIC_HEADER "include/octobass-core/BassProcessor.hpp;include/octobass-core/BusCompressor.hpp;include/octobass-core/Compressor.hpp;include/octobass-core/CompressorMode.hpp;include/octobass-core/Crossover.hpp;include/octobass-core/FETCompressor.hpp;include/octobass-core/GraphicEQ.hpp;include/octobass-core/NamProcessor.hpp;include/octobass-core/NoiseGate.hpp;include/octobass-core/OptoCompressor.hpp;include/octobass-core/Types.hpp;include/octobass-core/VCACompressor.hpp"
    POSITION_INDEPENDENT_CODE ON
)

option(BUILD_OCTOBASS_CORE_TESTS "Build octobass-core unit tests" OFF)
if(BUILD_OCTOBASS_CORE_TESTS)
    add_subdirectory(tests)
endif()
