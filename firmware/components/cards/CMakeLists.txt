idf_component_register(
    SRCS "card_mgr.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash json shared
)
