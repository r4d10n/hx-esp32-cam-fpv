idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_timer
        mipi_camera
        h264_encoder
        esp32_ipc
)
