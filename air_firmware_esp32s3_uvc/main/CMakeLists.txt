set(srcs
    dummy.cpp
    uvc_main.cpp
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS
        "../../components/esp32-camera/target/private_include/"
    REQUIRES
        esp_timer
        driver
        esp_psram
        nvs_flash
        heap
        uvc_device
)
