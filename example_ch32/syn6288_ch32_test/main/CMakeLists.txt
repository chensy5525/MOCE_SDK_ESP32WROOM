idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp_board ch32_uart_dynamic_gateway_final
             ch32_syn6288_gateway
)
