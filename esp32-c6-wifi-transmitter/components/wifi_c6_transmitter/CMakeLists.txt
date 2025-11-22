idf_component_register(
    SRCS "wifi_c6_transmitter.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_wifi
        esp_event
        esp_timer
        nvs_flash
        fec_encoder
)
