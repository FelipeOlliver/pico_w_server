# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(gcloud_mqtt_button_sensor_states C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(gcloud_mqtt_button_sensor_states gcloud_mqtt_button_sensor_states.c )

pico_set_program_name(gcloud_mqtt_button_sensor_states "gcloud_mqtt_button_sensor_states")
pico_set_program_version(gcloud_mqtt_button_sensor_states "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(gcloud_mqtt_button_sensor_states 0)
pico_enable_stdio_usb(gcloud_mqtt_button_sensor_states 1)

target_compile_definitions(gcloud_mqtt_button_sensor_states PRIVATE
    WIFI_SSID="WIFI_SSID"
    WIFI_PASSWORD="WIFI_PASSWORD"
    MQTT_SERVER="MQTT_SERVER"
    MQTT_USERNAME="MQTT_USERNAME"
    MQTT_PASSWORD="MQTT_PASSWORD"
)

# Add the standard library to the build
target_link_libraries(gcloud_mqtt_button_sensor_states
    pico_stdlib
    hardware_gpio
    hardware_adc
    hardware_pio
    hardware_dma
    onewire_library
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
)

# Add the standard include files to the build
target_include_directories(gcloud_mqtt_button_sensor_states PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

# Add any user requested libraries
target_link_libraries(gcloud_mqtt_button_sensor_states 
        pico_cyw43_arch_none
        ${CMAKE_CURRENT_LIST_DIR}
        ${PICO_SDK_PATH}/lib/lwip/src/include
        ${PICO_SDK_PATH}/lib/lwip/src/include/lwip
)

add_subdirectory(onewire_library)

pico_add_extra_outputs(gcloud_mqtt_button_sensor_states)

