idf_component_register(
    SRCS "com.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES cjson esp_event esp_http_server esp_netif esp_timer esp_wifi lwip mdns nvs_flash
)