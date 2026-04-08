cmake_minimum_required(VERSION 3.22)

# Use version from parent project (read from VERSION file)
if(NOT DEFINED OCTOBIR_VERSION)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../../VERSION" OCTOBIR_VERSION)
    string(STRIP "${OCTOBIR_VERSION}" OCTOBIR_VERSION)
endif()

project(OctobIR VERSION ${OCTOBIR_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Note: JUCE and juce-ui-shared are added by root CMakeLists.txt
# Note: octobir-core is added by root CMakeLists.txt

set(FORMATS VST3 AU)

juce_add_plugin(OctobIR
    COMPANY_NAME "October Production Co."
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Octb
    PLUGIN_CODE Obir
    FORMATS ${FORMATS}
    PRODUCT_NAME "OctobIR")

target_sources(OctobIR PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(OctobIR
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(OctobIR
    PRIVATE
        juce::juce_audio_utils
        octobir-core
        juce-ui-shared
        October_SharedResources
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(BUILD_OCTOBIR_JUCE_TESTS)
    add_subdirectory(tests)
endif()
