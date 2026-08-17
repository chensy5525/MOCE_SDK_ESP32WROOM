idf_component_register(
    SRCS "syn6288e_direct.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_board bsp_uart module_errors
)
