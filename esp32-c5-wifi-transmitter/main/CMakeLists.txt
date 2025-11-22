idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_timer
        esp32_ipc
        wifi_c5_transmitter
)
