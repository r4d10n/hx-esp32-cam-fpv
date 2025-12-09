idf_component_register(
    SRCS
        "src/video_ws_server.cpp"
    INCLUDE_DIRS
        "include"
    REQUIRES
        esp_http_server
)
