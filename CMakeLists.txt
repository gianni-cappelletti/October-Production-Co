cmake_minimum_required(VERSION 3.22)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION)
string(STRIP "${PROJECT_VERSION}" PROJECT_VERSION)

project(OPC-Monorepo VERSION ${PROJECT_VERSION})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Make version available to subprojects
set(OCTOBIR_VERSION ${PROJECT_VERSION} CACHE STRING "OctobIR version" FORCE)

# Plugin selection
option(BUILD_OCTOBIR "Build OctobIR plugin" ON)
option(BUILD_OCTOBIR_JUCE "Build OctobIR JUCE plugin" ON)
option(BUILD_OCTOBIR_VCV "Build OctobIR VCV plugin" ON)
option(BUILD_OCTOBASS "Build OctoBASS plugin" ON)
option(BUILD_OCTOBASS_JUCE "Build OctoBASS JUCE plugin" ON)

# Test selection
option(BUILD_OCTOBIR_CORE_TESTS "Build OctobIR core library tests" OFF)
option(BUILD_OCTOBIR_JUCE_TESTS "Build OctobIR JUCE plugin tests" OFF)
option(BUILD_OCTOBIR_VCV_TESTS "Build OctobIR VCV plugin tests" OFF)
option(BUILD_OCTOBASS_JUCE_TESTS "Build OctoBASS JUCE plugin tests" OFF)

# JUCE framework (shared by all JUCE plugins and their tests)
if(BUILD_OCTOBIR_JUCE OR BUILD_OCTOBASS_JUCE OR
   BUILD_OCTOBIR_JUCE_TESTS OR BUILD_OCTOBASS_JUCE_TESTS)
    add_subdirectory(third_party/JUCE)
    add_subdirectory(libs/juce-ui-shared)
endif()

# octobir-core is used by both OctobIR and OctoBASS (IR convolution in high-freq chain)
if(BUILD_OCTOBIR OR BUILD_OCTOBASS OR BUILD_OCTOBIR_CORE_TESTS)
    add_subdirectory(libs/octobir-core)
endif()

# OctobIR
if(BUILD_OCTOBIR_JUCE OR BUILD_OCTOBIR_JUCE_TESTS)
    add_subdirectory(plugins/octobir/juce)
endif()

if(BUILD_OCTOBIR_VCV OR BUILD_OCTOBIR_VCV_TESTS)
    add_subdirectory(plugins/octobir/vcv-rack)
endif()

# OctoBASS
# octobass-core will be added here in Phase 2
if(BUILD_OCTOBASS_JUCE OR BUILD_OCTOBASS_JUCE_TESTS)
    add_subdirectory(plugins/octobass/juce)
endif()
