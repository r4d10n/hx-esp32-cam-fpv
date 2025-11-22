idf_component_register(
    SRCS "usb_streamer.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_timer
        driver
)
