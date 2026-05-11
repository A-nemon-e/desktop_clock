idf_component_register(
    SRCS "framebuffer.c" "display_task.c" "font_renderer.c" 
         "effects/bg_fire.c" "effects/bg_code_rain.c" "effects/bg_water.c" 
         "effects/bg_life.c" "effects/bg_hourglass.c"
         "effects/bg_pong.c" "effects/bg_weather.c" "effects/fg_render.c"
    INCLUDE_DIRS "include" "fonts" "effects"
    REQUIRES freertos driver led_matrix i2c_mux config shared
)
