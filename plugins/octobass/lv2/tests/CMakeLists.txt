cmake_minimum_required(VERSION 3.15)

include(FetchContent)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(octobass-lv2-tests
  ${CMAKE_SOURCE_DIR}/tests/support/AllocationTracker.cpp
  FakeLv2Host.cpp
  Lv2BundleTests.cpp
  Lv2PluginTests.cpp
  ReferenceChain.cpp
  $<TARGET_OBJECTS:octobass-lv2-objects>
)

target_link_libraries(octobass-lv2-tests
  PRIVATE
    octobass-lv2-objects
    octobass-core
    gtest_main
)

add_dependencies(octobass-lv2-tests octobass-lv2-bundle)

target_include_directories(octobass-lv2-tests PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)

target_compile_definitions(octobass-lv2-tests
  PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
    LV2_BUNDLE_SOURCE_DIR="${OCTOBASS_LV2_BUNDLE_SOURCE_DIR}"
    LV2_BUNDLE_BUILD_DIR="${OCTOBASS_LV2_BUNDLE_DIR}"
    MONOREPO_VERSION_FILE="${CMAKE_SOURCE_DIR}/VERSION"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(octobass-lv2-tests PRIVATE -Wall -Wextra)
endif()

include(GoogleTest)
gtest_discover_tests(octobass-lv2-tests
    DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT 30
)
