cmake_minimum_required(VERSION 3.13)

set(PROJECT pico-cec)
set(FAMILY rp2040)

# Pull in SDK (must be before project)
include(pico-sdk/pico_sdk_init.cmake)
include(pico-sdk/lib/tinyusb/hw/bsp/family_support.cmake)
family_get_project_name(PROJECT ${PROJECT})

# Pull in FreeRTOS
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040)

project(app C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(${PROJECT}
  src/freertos_hook.c
  src/hdmi-cec.c
  src/main.c
  src/usb_descriptors.c
  src/usb_hid.c)
target_include_directories(${PROJECT} PRIVATE
  ${PROJECT_SOURCE_DIR}/include)

target_compile_options(${PROJECT} PRIVATE
  -UCFG_TUSB_OS
  -DCFG_TUSB_OS=OPT_OS_FREERTOS
  -Wall -Werror)

target_link_libraries(${PROJECT}
  pico_stdlib
  pico_unique_id
  tinyusb_device
  tinyusb_board
  FreeRTOS-Kernel
  FreeRTOS-Kernel-Heap3)

pico_add_extra_outputs(${PROJECT})
pico_set_binary_type(${PROJECT} copy_to_ram)
#pico_enable_stdio_usb(${PROJECT} 1)
#pico_enable_stdio_uart(${PROJECT} 0)
