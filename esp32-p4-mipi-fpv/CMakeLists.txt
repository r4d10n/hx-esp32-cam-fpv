# ESP32-P4 High-Resolution FPV System

cmake_minimum_required(VERSION 3.16)

# Set the project name
set(PROJECT_NAME "esp32-p4-mipi-fpv")

# Include ESP-IDF CMake
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(${PROJECT_NAME})
