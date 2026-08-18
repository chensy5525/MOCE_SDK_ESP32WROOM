idf_component_register(
    SRCS "e104_bt01_direct_uart.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_uart esp_timer
)
