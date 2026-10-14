cmake_minimum_required(VERSION 3.22)
project(juce-ui-shared)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

juce_add_binary_data(October_SharedResources SOURCES
    Resources/Fonts/CourierPrime-Regular.ttf
    Resources/Fonts/PressStart2P-Regular.ttf
    Resources/Images/OctoberLogo.png)
set_target_properties(October_SharedResources PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_library(juce-ui-shared STATIC
    Source/BrowseFolderMemory.cpp
    Source/OctoberLookAndFeel.cpp)
set_target_properties(juce-ui-shared PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(juce-ui-shared
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_link_libraries(juce-ui-shared
    PUBLIC
        juce::juce_gui_basics
        October_SharedResources)

target_compile_definitions(juce-ui-shared
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
