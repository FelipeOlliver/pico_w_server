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

project(hivemq_pin_sensor C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(hivemq_pin_sensor 
        hivemq_pin_sensor.c 
        ${PICO_SDK_PATH}/lib/lwip/src/apps/altcp_tls/altcp_tls_mbedtls.c
)

pico_set_program_name(hivemq_pin_sensor "hivemq_pin_sensor")
pico_set_program_version(hivemq_pin_sensor "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(hivemq_pin_sensor 0)
pico_enable_stdio_usb(hivemq_pin_sensor 1)

target_compile_definitions(hivemq_pin_sensor PRIVATE
    WIFI_SSID="WIFI_SSID"
    WIFI_PASSWORD=" WIFI_PASSWORD"
    MQTT_SERVER="broker.hivemq.com"
    MQTT_PORT=1883
)

# Add the standard library to the build
target_link_libraries(hivemq_pin_sensor
    pico_stdlib
    hardware_gpio
    hardware_adc
    hardware_pio
    hardware_dma
    onewire_library
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
    pico_mbedtls
    pico_lwip_mbedtls
)

# Add the standard include files to the build
target_include_directories(hivemq_pin_sensor PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

# Add any user requested libraries
target_link_libraries(hivemq_pin_sensor 
        pico_cyw43_arch_none
        ${CMAKE_CURRENT_LIST_DIR}
        ${PICO_SDK_PATH}/lib/lwip/src/include
        ${PICO_SDK_PATH}/lib/lwip/src/include/lwip
        ${PICO_SDK_PATH}/lib/lwip/src/apps/altcp_tls
)

add_subdirectory(onewire_library)

pico_add_extra_outputs(hivemq_pin_sensor)

