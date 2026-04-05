cmake_minimum_required(VERSION 3.22)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(octobir-plugin-tests
    TestMain.cpp
    PluginProcessorTests.cpp
    PluginAudioTests.cpp
    ../Source/PluginProcessor.cpp
    ../Source/PluginEditor.cpp
)

target_include_directories(octobir-plugin-tests
    PRIVATE
        ../Source
)

target_include_directories(octobir-plugin-tests
    SYSTEM PRIVATE
        ${CMAKE_SOURCE_DIR}/third_party
        ${CMAKE_SOURCE_DIR}/third_party/WDL/WDL
)

target_compile_definitions(octobir-plugin-tests
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JucePlugin_Name="OctobIR"
        JucePlugin_VersionString="${PROJECT_VERSION}"
        TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

target_link_libraries(octobir-plugin-tests
    PRIVATE
        octobir-core
        juce-ui-shared
        October_SharedResources
        juce::juce_audio_utils
        GTest::gtest
)

include(GoogleTest)
gtest_discover_tests(octobir-plugin-tests)
