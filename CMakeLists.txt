cmake_minimum_required(VERSION 3.22)

# Read version from VERSION file (may contain prerelease suffix, e.g. "2.0.0-beta.1")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" OPC_VERSION_FULL)
string(STRIP "${OPC_VERSION_FULL}" OPC_VERSION_FULL)

# CMake project(VERSION ...) only accepts numeric MAJOR.MINOR.PATCH
string(REGEX REPLACE "-.*$" "" OPC_VERSION_NUMERIC "${OPC_VERSION_FULL}")

project(OPC-Monorepo VERSION ${OPC_VERSION_NUMERIC})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# WDL (third_party) static-asserts that `char` is signed. On ARM-family targets
# the platform ABI makes `char` unsigned by default with GCC/Clang, which trips
# the assertion. Force signed `char` everywhere so x86 and ARM agree; this is a
# no-op on x86 where signed is already the default. MSVC also defaults to
# signed char, so this only needs to be set for GCC/Clang.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fsigned-char)
endif()

# Make version available to subprojects (numeric for CMake, full for display/packaging)
set(OPC_VERSION_FULL "${OPC_VERSION_FULL}" CACHE STRING "Full version string" FORCE)
set(OCTOBIR_VERSION ${OPC_VERSION_NUMERIC} CACHE STRING "OctobIR version (numeric)" FORCE)
set(OCTOBASS_VERSION ${OPC_VERSION_NUMERIC} CACHE STRING "OctoBASS version (numeric)" FORCE)

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
option(BUILD_OCTOBASS_CORE_TESTS "Build OctoBASS core library tests" OFF)

# JUCE framework (shared by all JUCE plugins and their tests)
if(BUILD_OCTOBIR_JUCE OR BUILD_OCTOBASS_JUCE OR
   BUILD_OCTOBIR_JUCE_TESTS OR BUILD_OCTOBASS_JUCE_TESTS)
    add_subdirectory(third_party/JUCE)
    add_subdirectory(libs/juce-ui-shared)
endif()

# octobir-core is used by both OctobIR and OctoBASS (IR convolution in high-freq chain)
if(BUILD_OCTOBIR OR BUILD_OCTOBASS OR BUILD_OCTOBIR_CORE_TESTS OR BUILD_OCTOBASS_CORE_TESTS)
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
if(BUILD_OCTOBASS OR BUILD_OCTOBASS_JUCE OR BUILD_OCTOBASS_CORE_TESTS)
    add_subdirectory(libs/octobass-core)
endif()

if(BUILD_OCTOBASS_JUCE OR BUILD_OCTOBASS_JUCE_TESTS)
    add_subdirectory(plugins/octobass/juce)
endif()
