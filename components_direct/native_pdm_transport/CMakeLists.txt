idf_component_register(
    SRCS "native_pdm_transport.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_gpio esp_driver_i2s freertos
)
