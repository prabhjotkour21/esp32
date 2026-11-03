idf_component_register(
    SRCS "com.c" "eeprom.c" "sd_card.c" "wifi.c" "rtc.c" "uart.c" "app.c" "main.c"
    INCLUDE_DIRS "."
    REQUIRES cjson esp_event esp_http_server esp_netif esp_timer esp_wifi lwip mdns nvs_flash fatfs sdmmc esp_driver_sdspi esp_driver_spi esp_driver_uart
)