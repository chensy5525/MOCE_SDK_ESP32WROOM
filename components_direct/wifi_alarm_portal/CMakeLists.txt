idf_component_register(
    SRCS "wifi_alarm_portal.c" "portal_dns_server.c"
    INCLUDE_DIRS "include"
    EMBED_TXTFILES "web/index.html"
    REQUIRES esp_event esp_http_server esp_netif esp_wifi freertos nvs_flash
)
