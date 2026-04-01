cmake_minimum_required(VERSION 3.15)

# VCV Rack Plugin Build Wrapper
# This wraps the existing Makefile-based build system since VCV Rack
# requires using their plugin.mk framework

# Find RACK_DIR (defaults to ../.., can be overridden via environment variable)
if(NOT DEFINED ENV{RACK_DIR})
    set(RACK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../..")
else()
    set(RACK_DIR $ENV{RACK_DIR})
endif()

# Determine the plugin extension based on platform
if(APPLE)
    set(PLUGIN_EXTENSION "dylib")
elseif(WIN32)
    set(PLUGIN_EXTENSION "dll")
else()
    set(PLUGIN_EXTENSION "so")
endif()

set(VCV_PLUGIN_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/plugin.${PLUGIN_EXTENSION}")
set(VCV_DIST_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/dist")

# Custom target to build the VCV plugin using make
add_custom_target(vcv-plugin ALL
    COMMAND ${CMAKE_MAKE_PROGRAM}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Building VCV Rack plugin using Makefile"
    VERBATIM
)

# Custom target to clean VCV plugin build artifacts
add_custom_target(vcv-plugin-clean
    COMMAND ${CMAKE_MAKE_PROGRAM} clean
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Cleaning VCV Rack plugin build artifacts"
    VERBATIM
)

# Custom target to create distribution package
add_custom_target(vcv-plugin-dist
    COMMAND ${CMAKE_MAKE_PROGRAM} dist
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Creating VCV Rack plugin distribution package"
    VERBATIM
)

# Custom target to install VCV plugin
add_custom_target(vcv-plugin-install
    COMMAND ${CMAKE_MAKE_PROGRAM} install
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Installing VCV Rack plugin to Rack directory"
    VERBATIM
)

# Custom target for code formatting
add_custom_target(vcv-plugin-format
    COMMAND ${CMAKE_MAKE_PROGRAM} format
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Formatting VCV Rack plugin code with clang-format"
    VERBATIM
)

# Custom target for format checking
add_custom_target(vcv-plugin-format-check
    COMMAND ${CMAKE_MAKE_PROGRAM} format-check
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking VCV Rack plugin code formatting"
    VERBATIM
)

# Make the main target depend on the core library if it exists
if(TARGET octobir-core)
    add_dependencies(vcv-plugin octobir-core)
endif()

if(BUILD_VCV_TESTS)
    add_subdirectory(tests)
endif()

# Add custom clean target to the overall clean
set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
    ${VCV_PLUGIN_OUTPUT}
    "${CMAKE_CURRENT_SOURCE_DIR}/build"
)
