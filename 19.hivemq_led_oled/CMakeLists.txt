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

project(hivemq_led_oled C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

# Define o nome do executável que será gerado
set(EXECUTABLE_NAME hivemq_led_oled)

add_executable(${EXECUTABLE_NAME}     
    hivemq_led_oled.c
    oled_driver.c
)
pico_set_program_name(${EXECUTABLE_NAME} "hivemq_led_oled")
pico_set_program_version(${EXECUTABLE_NAME} "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${EXECUTABLE_NAME} 0)
pico_enable_stdio_usb(${EXECUTABLE_NAME} 1)

# Add the standard library to the build
target_link_libraries(${EXECUTABLE_NAME}
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_lwip_mqtt
    hardware_i2c
)

# Add the standard include files to the build
target_include_directories(${EXECUTABLE_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# Add any user requested libraries
target_link_libraries(${EXECUTABLE_NAME} 
        pico_cyw43_arch_none
        )

pico_add_extra_outputs(${EXECUTABLE_NAME})

