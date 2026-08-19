idf_component_register(
    SRCS "wifi_alarm_portal.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event esp_http_server esp_netif esp_wifi freertos nvs_flash
)
