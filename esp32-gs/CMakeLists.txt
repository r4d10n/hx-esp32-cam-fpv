# ESP32 FPV Ground Station
# Captures FPV packets in promiscuous mode and serves via web interface

cmake_minimum_required(VERSION 3.16)

# Set default IDF target if not specified
if(NOT DEFINED ENV{IDF_TARGET})
    set(ENV{IDF_TARGET} "esp32s3")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32-fpv-gs)
