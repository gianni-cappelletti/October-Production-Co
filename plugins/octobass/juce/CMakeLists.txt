cmake_minimum_required(VERSION 3.22)

# Use version from parent project (read from VERSION file)
if(NOT DEFINED OCTOBASS_VERSION)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../../../VERSION" OCTOBASS_VERSION)
    string(STRIP "${OCTOBASS_VERSION}" OCTOBASS_VERSION)
endif()

project(OctoBASS VERSION ${OCTOBASS_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Note: JUCE and juce-ui-shared are added by root CMakeLists.txt
# Note: octobir-core is added by root CMakeLists.txt

set(FORMATS VST3 AU)

juce_add_plugin(OctoBASS
    COMPANY_NAME "October Production Co."
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Octb
    PLUGIN_CODE Obas
    FORMATS ${FORMATS}
    PRODUCT_NAME "OctoBASS")

target_sources(OctoBASS PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(OctoBASS
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(OctoBASS
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        octobass-core
        juce-ui-shared
        October_SharedResources
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(BUILD_OCTOBASS_JUCE_TESTS)
    add_subdirectory(tests)
endif()
