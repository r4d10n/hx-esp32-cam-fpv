# UVC Device Component for ESP32-S3
# This component provides USB Video Class (UVC) functionality

set(UVC_SRCS
    "uvc_device.c"
    "usb_descriptors.c"
)

idf_component_register(
    SRCS ${UVC_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES driver esp_rom freertos heap
    PRIV_REQUIRES esp_timer
)

# Check for TinyUSB in ESP-IDF components directory
set(TINYUSB_SRC_DIR "${IDF_PATH}/components/tinyusb/tinyusb/src")

if(EXISTS "${TINYUSB_SRC_DIR}")
    message(STATUS "Found TinyUSB at: ${TINYUSB_SRC_DIR}")

    # TinyUSB core sources
    set(TINYUSB_SRCS
        "${TINYUSB_SRC_DIR}/tusb.c"
        "${TINYUSB_SRC_DIR}/common/tusb_fifo.c"
        "${TINYUSB_SRC_DIR}/device/usbd.c"
        "${TINYUSB_SRC_DIR}/device/usbd_control.c"
    )

    # Video class source (if available)
    if(EXISTS "${TINYUSB_SRC_DIR}/class/video/video_device.c")
        list(APPEND TINYUSB_SRCS "${TINYUSB_SRC_DIR}/class/video/video_device.c")
        message(STATUS "TinyUSB Video class found")
    else()
        message(WARNING "TinyUSB Video class not found - using stub")
    endif()

    # ESP32-S3 DCD driver
    set(DCD_PATH "${TINYUSB_SRC_DIR}/portable/espressif/esp32sx/dcd_esp32sx.c")
    if(EXISTS "${DCD_PATH}")
        list(APPEND TINYUSB_SRCS "${DCD_PATH}")
    endif()

    target_sources(${COMPONENT_LIB} PRIVATE ${TINYUSB_SRCS})

    target_include_directories(${COMPONENT_LIB} PUBLIC
        "${TINYUSB_SRC_DIR}"
        "${TINYUSB_SRC_DIR}/common"
    )

    # Also include ESP-IDF's TinyUSB additions if available
    set(TINYUSB_ADDITIONS "${IDF_PATH}/components/tinyusb/additions/include")
    if(EXISTS "${TINYUSB_ADDITIONS}")
        target_include_directories(${COMPONENT_LIB} PUBLIC "${TINYUSB_ADDITIONS}")
    endif()
else()
    message(WARNING "TinyUSB not found in ESP-IDF. USB functionality disabled.")
endif()

# Compiler options
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-format
    -Wno-maybe-uninitialized
    -Wno-unused-function
)
