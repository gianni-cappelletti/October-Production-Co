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

add_executable(octobir-vcv-tests
    TestMain.cpp
    VcvModuleTests.cpp
    VcvAudioTests.cpp
)

target_include_directories(octobir-vcv-tests
    PRIVATE
        # Contains rack_stub.hpp; allows #include "rack_stub.hpp" from src/opc-vcv-ir.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}
        # Access plugin.hpp stub path (not needed in headless mode but keeps include structure clean)
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_include_directories(octobir-vcv-tests
    SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party
        ${CMAKE_SOURCE_DIR}/third_party/WDL/WDL
)

target_compile_definitions(octobir-vcv-tests
    PRIVATE
        OCTOBIR_VCV_HEADLESS_TEST=1
        TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

target_compile_options(octobir-vcv-tests
    PRIVATE
        $<$<CXX_COMPILER_ID:Clang,GNU>:-Wall -Wextra -Wno-unused-parameter>
)

target_link_libraries(octobir-vcv-tests
    PRIVATE
        octobir-core
        GTest::gtest
)

include(GoogleTest)
gtest_discover_tests(octobir-vcv-tests)
