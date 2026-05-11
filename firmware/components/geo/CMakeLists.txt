idf_component_register(
    SRCS "geo_pipeline.c"
    INCLUDE_DIRS "include"
    REQUIRES shared http_client ntp json
)
