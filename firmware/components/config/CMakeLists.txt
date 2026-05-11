idf_component_register(
    SRCS "config_sync.c"
    INCLUDE_DIRS "include"
    REQUIRES driver nvs_flash json mqtt_client cards
)
