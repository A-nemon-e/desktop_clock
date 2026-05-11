idf_component_register(
    SRCS "ota_handler.c"
    INCLUDE_DIRS "include"
    REQUIRES app_update esp_https_ota json shared spi_flash
)
