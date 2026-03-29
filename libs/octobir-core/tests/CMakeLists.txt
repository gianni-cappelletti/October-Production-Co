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

add_executable(octobir-core-tests
  IRProcessorTests.cpp
  IRProcessorLogicTests.cpp
  IRLoaderTests.cpp
  LatencyCompensationTests.cpp
  DynamicModeTests.cpp
  SidechainTests.cpp
  ComponentTests.cpp
)

target_link_libraries(octobir-core-tests
  PRIVATE
    octobir-core
    gtest_main
)

target_compile_definitions(octobir-core-tests
  PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

target_include_directories(octobir-core-tests
  SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/WDL/WDL
)

include(GoogleTest)
gtest_discover_tests(octobir-core-tests)
