idf_component_register(
    SRCS "msm261dgt003_direct_pdm.c"
    INCLUDE_DIRS "include"
    REQUIRES bsp_i2s esp_driver_gpio log
)
