add_executable(octobass-core-bench OctobassCoreBench.cpp)

target_link_libraries(octobass-core-bench PRIVATE octobass-core)

target_include_directories(octobass-core-bench
    SYSTEM PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../third_party
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(octobass-core-bench PRIVATE -Wall -Wextra)
endif()
