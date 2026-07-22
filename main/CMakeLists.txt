idf_component_register(
    SRCS "main.c" "config.c" "telegram.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_gpio esp_wifi nvs_flash esp_netif esp_event led_strip
                  esp_http_client esp-tls json esp_driver_tsens esp_timer
    )
