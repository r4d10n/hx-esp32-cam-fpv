# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/tmp/esp-idf-v5.3/components/bootloader/subproject"
  "/home/user/hx-esp32-cam-fpv/esp32-c5-wifi-transmitter/build/bootloader"
  "/home/user/hx-esp32-cam-fpv/esp32-c5-wifi-transmitter/build/bootloader-prefix"
  "/home/user/hx-esp32-cam-fpv/esp32-c5-wifi-transmitter/build/bootloader-prefix/tmp"
  "/home/user/hx-esp32-cam-fpv/esp32-c5-wifi-transmitter/build/bootloader-prefix/src/bootloader-stamp"
  "/home/user/hx-esp32-cam-fpv/esp32-c5-wifi-transmitter/build/bootloader-prefix/src"
  "/home/user/hx-esp32-cam-fpv/esp32-c5-wifi-transmitter/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/user/hx-esp32-cam-fpv/esp32-c5-wifi-transmitter/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/user/hx-esp32-cam-fpv/esp32-c5-wifi-transmitter/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
