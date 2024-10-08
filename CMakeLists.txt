cmake_minimum_required(VERSION 3.13)

set(PROJECT pico-cec)
set(FAMILY rp2040)
set(BOARD "pico_sdk") # Configure TinyUSB use pico-sdk board configuration
set(PICO_BOARD "seeed_xiao_rp2040" CACHE STRING "Raspberry Pi Pico board type.")
#set(PICO_BOARD "pico" CACHE STRING "Raspberry Pi Pico board type.")

# Include pico-sdk and tinyusb (must be before project)
include(pico-sdk/pico_sdk_init.cmake)
include(pico-sdk/lib/tinyusb/hw/bsp/family_support.cmake)
family_get_project_name(PROJECT ${PROJECT})

project(${PROJECT}
  DESCRIPTION "HDMI CEC to USB HID keyboard adapter."
  LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Include FreeRTOS kernel
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040)

# Initialize the SDK
pico_sdk_init()

add_compile_options(-Wall -Werror)

set(TCLI_SOURCE_DIR ${PROJECT_SOURCE_DIR}/tcli)

add_library(tcli STATIC
  ${TCLI_SOURCE_DIR}/source/tcli.c
  ${TCLI_SOURCE_DIR}/source/tclie.c)

target_include_directories(tcli PUBLIC
  ${TCLI_SOURCE_DIR}/include)

target_compile_definitions(tcli PUBLIC
  TCLI_COMPLETE=1
  TCLIE_ENABLE_USERS=0
  TCLIE_PATTERN_MATCH=1)

target_compile_options(tcli PRIVATE
  -Wno-stringop-truncation)

add_executable(${PROJECT}
  src/freertos_hook.c
  src/hdmi-cec.c
  src/hdmi-ddc.c
  src/main.c
  src/usb_cdc.c
  src/usb_descriptors.c
  src/usb_hid.c)

target_include_directories(${PROJECT} PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/tusb)

set(CEC_PIN "3" CACHE STRING "GPIO pin for HDMI CEC.")
set(PICO_CEC_VERSION "unknown" CACHE STRING "Pico-CEC version string.")

set_source_files_properties(src/hdmi-cec.c PROPERTIES COMPILE_DEFINITIONS
  "CEC_PIN=${CEC_PIN}")

set_source_files_properties(src/usb_cdc.c PROPERTIES COMPILE_DEFINITIONS
  "PICO_CEC_VERSION=\"${PICO_CEC_VERSION}\"")

# Undefine TinyUSB built-in OS, redefined in our tusb_config.h
target_compile_options(${PROJECT} PRIVATE
  -UCFG_TUSB_OS)

target_link_libraries(${PROJECT}
  pico_stdlib
  pico_unique_id
  hardware_i2c
  tinyusb_device
  tinyusb_board
  FreeRTOS-Kernel
  tcli)

pico_add_extra_outputs(${PROJECT})
pico_set_binary_type(${PROJECT} copy_to_ram)

# disable stdio and uart
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 0)

include(debug.cmake)
