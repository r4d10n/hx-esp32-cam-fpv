idf_component_register(
    SRCS
        "main.c"
        "wifi_manager.c"
        "packet_rx.c"
        "frame_buffer.c"
        "web_server.c"
        "config_manager.c"
        "usb_device.c"
        "usb_uvc.c"
    INCLUDE_DIRS
        "."
    EMBED_FILES
        "../frontend/index.html"
        "../frontend/app.js"
        "../frontend/style.css"
    REQUIRES
        esp_wifi
        nvs_flash
        esp_http_server
        esp_netif
        esp_timer
        usb
        freertos
)

# UVC-only builds use usb_device_uvc component which handles TinyUSB internally
# No additional include paths needed
