idf_component_register(
    SRCS "wifi_receiver.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_wifi
        esp_event
        esp_timer
        nvs_flash
)
