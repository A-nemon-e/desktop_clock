idf_component_register(
    SRCS "app_main.c" "gpio_test.c" "i2c_scan.c" "render_task.c"
    INCLUDE_DIRS "."
    REQUIRES config log shared display led_matrix i2c_mux sensors button
              wifi_mgr http_client ntp geo mqtt_client cards thin_mode ota version
)
