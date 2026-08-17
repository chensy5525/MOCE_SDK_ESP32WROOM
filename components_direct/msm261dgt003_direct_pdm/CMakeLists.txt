idf_component_register(
    SRCS "msm261dgt003_direct_pdm.c"
    INCLUDE_DIRS "include"
    REQUIRES native_pdm_transport esp_driver_gpio esp_driver_i2s log
)
