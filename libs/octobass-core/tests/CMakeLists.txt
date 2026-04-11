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

add_executable(octobass-core-tests
  CompressorTests.cpp
  CompressorAudioTests.cpp
  CrossoverTests.cpp
  NoiseGateTests.cpp
  BassProcessorTests.cpp
  BassProcessorAudioTests.cpp
  NamProcessorTests.cpp
)

target_link_libraries(octobass-core-tests
  PRIVATE
    octobass-core
    gtest_main
)

target_compile_definitions(octobass-core-tests
  PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

target_include_directories(octobass-core-tests
  SYSTEM PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/WDL/WDL
)

include(GoogleTest)
gtest_discover_tests(octobass-core-tests)
