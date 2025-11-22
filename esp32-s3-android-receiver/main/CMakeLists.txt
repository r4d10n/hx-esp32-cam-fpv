idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_timer
        wifi_receiver
        fec_decoder
        usb_streamer
        packet_handler
        stats_tracker
)
