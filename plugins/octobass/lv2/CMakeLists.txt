cmake_minimum_required(VERSION 3.22)

set(LV2_HEADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party/lv2/include)
set(OCTOBASS_LV2_BUNDLE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bundle)
set(OCTOBASS_LV2_BUNDLE_DIR ${CMAKE_CURRENT_BINARY_DIR}/OctoBASS.lv2)

add_library(octobass-lv2-objects OBJECT
    src/Lv2Entry.cpp
    src/OctoBassLv2.cpp
    src/PluginTtl.cpp
    src/PortTable.cpp
)

target_include_directories(octobass-lv2-objects PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(octobass-lv2-objects SYSTEM PUBLIC ${LV2_HEADERS_DIR})
target_link_libraries(octobass-lv2-objects PUBLIC octobass-core)

set_target_properties(octobass-lv2-objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(octobass-lv2-objects PRIVATE -Wall -Wextra)
endif()

add_library(octobass-lv2 MODULE $<TARGET_OBJECTS:octobass-lv2-objects>)
target_link_libraries(octobass-lv2 PRIVATE octobass-core)

set_target_properties(octobass-lv2 PROPERTIES
    PREFIX ""
    OUTPUT_NAME octobass
    LIBRARY_OUTPUT_DIRECTORY ${OCTOBASS_LV2_BUNDLE_DIR}
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(octobass-lv2 PRIVATE -Wl,--no-undefined -Wl,--exclude-libs,ALL)
endif()

if(NOT OPC_VERSION_NUMERIC MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
    message(FATAL_ERROR "VERSION must be MAJOR.MINOR.PATCH, got '${OPC_VERSION_NUMERIC}'")
endif()
set(OCTOBASS_VERSION_MAJOR ${CMAKE_MATCH_1})
set(OCTOBASS_VERSION_MINOR ${CMAKE_MATCH_2})
set(OCTOBASS_VERSION_PATCH ${CMAKE_MATCH_3})
if(OCTOBASS_VERSION_MINOR GREATER_EQUAL 100)
    message(FATAL_ERROR "LV2 version encoding requires MINOR < 100, got ${OCTOBASS_VERSION_MINOR}")
endif()
math(EXPR OCTOBASS_LV2_MINOR_VERSION "2 * (${OCTOBASS_VERSION_MAJOR} * 100 + ${OCTOBASS_VERSION_MINOR})")
math(EXPR OCTOBASS_LV2_MICRO_VERSION "2 * ${OCTOBASS_VERSION_PATCH}")
message(STATUS "OctoBASS LV2 version ${OCTOBASS_LV2_MINOR_VERSION}.${OCTOBASS_LV2_MICRO_VERSION} (VERSION ${OPC_VERSION_NUMERIC})")

set(OCTOBASS_LV2_BINARY "octobass${CMAKE_SHARED_MODULE_SUFFIX}")
configure_file(${OCTOBASS_LV2_BUNDLE_SOURCE_DIR}/manifest.ttl.in
               ${OCTOBASS_LV2_BUNDLE_DIR}/manifest.ttl @ONLY)

add_custom_target(octobass-lv2-bundle ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${OCTOBASS_LV2_BUNDLE_SOURCE_DIR}/octobass.ttl
            ${OCTOBASS_LV2_BUNDLE_SOURCE_DIR}/presets.ttl
            ${OCTOBASS_LV2_BUNDLE_SOURCE_DIR}/modgui.ttl
            ${OCTOBASS_LV2_BUNDLE_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${OCTOBASS_LV2_BUNDLE_SOURCE_DIR}/modgui
            ${OCTOBASS_LV2_BUNDLE_DIR}/modgui
    DEPENDS octobass-lv2
    COMMENT "Assembling OctoBASS.lv2 bundle"
)

if(NOT CMAKE_CROSSCOMPILING)
    add_executable(octobass-lv2-ttlgen src/TtlGenerator.cpp src/PluginTtl.cpp src/PortTable.cpp)
    target_include_directories(octobass-lv2-ttlgen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_include_directories(octobass-lv2-ttlgen SYSTEM PRIVATE ${LV2_HEADERS_DIR})
    target_link_libraries(octobass-lv2-ttlgen PRIVATE octobass-core)
endif()

if(BUILD_OCTOBASS_LV2_MODGUI_RENDERER)
    juce_add_console_app(octobass-modgui-renderer PRODUCT_NAME "octobass-modgui-renderer")
    target_sources(octobass-modgui-renderer PRIVATE tools/ModguiAssetRenderer.cpp)
    target_link_libraries(octobass-modgui-renderer PRIVATE juce-ui-shared juce::juce_recommended_config_flags)
    target_compile_definitions(octobass-modgui-renderer PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
endif()

option(BUILD_OCTOBASS_LV2_TESTS "Build OctoBASS LV2 plugin tests" OFF)
if(BUILD_OCTOBASS_LV2_TESTS)
    add_subdirectory(tests)
endif()
