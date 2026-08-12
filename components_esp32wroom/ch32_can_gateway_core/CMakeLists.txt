idf_component_register(
    SRCS "ch32_can_gateway_core.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board driver esp_timer freertos
)
