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

add_executable(${PROJECT}
  src/freertos_hook.c
  src/hdmi-cec.c
  src/hdmi-edid.c
  src/main.c
  src/usb_descriptors.c
  src/usb_hid.c)

target_include_directories(${PROJECT} PRIVATE
  ${PROJECT_SOURCE_DIR}/include)

set(CEC_PIN "3" CACHE STRING "GPIO pin for HDMI CEC.")

set_source_files_properties(src/hdmi-cec.c PROPERTIES COMPILE_DEFINITIONS
  "CEC_PIN=${CEC_PIN}")

target_compile_options(${PROJECT} PRIVATE
  -Wall -Werror)

# Undefine TinyUSB built-in OS, redefined in our tusb_config.h
target_compile_options(tinyusb_common_base INTERFACE
  -UCFG_TUSB_OS)

target_link_libraries(${PROJECT}
  pico_stdlib
  pico_unique_id
  hardware_i2c
  tinyusb_device
  tinyusb_board
  FreeRTOS-Kernel)

pico_add_extra_outputs(${PROJECT})
pico_set_binary_type(${PROJECT} copy_to_ram)

# disable stdio and uart
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 0)
