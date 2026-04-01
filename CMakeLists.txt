cmake_minimum_required(VERSION 3.15)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION)
string(STRIP "${PROJECT_VERSION}" PROJECT_VERSION)

project(OctobIR-Monorepo VERSION ${PROJECT_VERSION})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Make version available to subprojects
set(OCTOBIR_VERSION ${PROJECT_VERSION} CACHE STRING "OctobIR version" FORCE)

option(BUILD_CORE_LIB "Build octobir-core library" ON)
option(BUILD_JUCE_PLUGIN "Build JUCE plugin (VST3/AU/Standalone)" ON)
option(BUILD_VCV_PLUGIN "Build VCV Rack plugin" ON)
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_VCV_TESTS "Build VCV Rack plugin unit tests" OFF)

if(BUILD_CORE_LIB)
    add_subdirectory(libs/octobir-core)
endif()

if(BUILD_JUCE_PLUGIN)
    add_subdirectory(plugins/juce-multiformat)
endif()

if(BUILD_VCV_PLUGIN)
    add_subdirectory(plugins/vcv-rack)
elseif(BUILD_VCV_TESTS)
    add_subdirectory(plugins/vcv-rack/tests)
endif()
