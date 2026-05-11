# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PROJECT_NAME clock_fw)

# Extra component directories
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_SOURCE_DIR}/components/config
    ${CMAKE_SOURCE_DIR}/components/shared
    ${CMAKE_SOURCE_DIR}/components/display
    ${CMAKE_SOURCE_DIR}/components/led_matrix
    ${CMAKE_SOURCE_DIR}/components/i2c_mux
    ${CMAKE_SOURCE_DIR}/components/sensors
    ${CMAKE_SOURCE_DIR}/components/button
    ${CMAKE_SOURCE_DIR}/components/wifi_mgr
    ${CMAKE_SOURCE_DIR}/components/http_client
    ${CMAKE_SOURCE_DIR}/components/ntp
    ${CMAKE_SOURCE_DIR}/components/geo
    ${CMAKE_SOURCE_DIR}/components/mqtt_client
    ${CMAKE_SOURCE_DIR}/components/cards
    ${CMAKE_SOURCE_DIR}/components/thin_mode
    ${CMAKE_SOURCE_DIR}/components/ota
    ${CMAKE_SOURCE_DIR}/components/version
)

project(${PROJECT_NAME})
