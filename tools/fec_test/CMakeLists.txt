cmake_minimum_required(VERSION 3.10)
project(fec_test)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    main.cpp
    pcap_parser.cpp
    ../../components/common/fec.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/common
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/gs_fec_decoder/include
)

# Build executable
add_executable(fec_test ${SOURCES})

# Platform-specific settings
if(WIN32)
    target_compile_definitions(fec_test PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(fec_test pthread)
endif()

# Installation
install(TARGETS fec_test DESTINATION bin)
