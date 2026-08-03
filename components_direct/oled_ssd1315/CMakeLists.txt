idf_component_register(
    SRCS "oled_ssd1315.c" "oled_ssd1315_font.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES bsp_i2c
)
