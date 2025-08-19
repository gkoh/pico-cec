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
cmake_minimum_required(VERSION 3.13)

set(PROJECT pico-cec)
set(FAMILY rp2040)
set(BOARD "pico_sdk") # Configure TinyUSB use pico-sdk board configuration
#set(PICO_BOARD "seeed_xiao_rp2040" CACHE STRING "Raspberry Pi Pico board type.")

#set(PICO_BOARD "waveshare_rp2040_zero" CACHE STRING "Raspberry Pi Pico board type.")
set(PICO_BOARD "vcc_gnd_yd_2040" CACHE STRING "Raspberry Pi Pico board type.")
#set(PICO_BOARD "pico" CACHE STRING "Raspberry Pi Pico board type.")

set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include/boards)

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

# Include CRC library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/crc)

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
  src/blink.c
  lib/cec_ulib/cec-config.c
  lib/cec_ulib/cec-cmd.c
  lib/cec_ulib/cec-frame.c
  lib/cec_ulib/cec-hal.c
#  lib/cec_ulib/cec-id.c
  lib/cec_ulib/cec-log.c
  lib/cec_ulib/cec-task.c
  lib/cec_ulib/cec-user.c
  lib/cec_ulib/cec-util.c
  src/config-keymap.c
  src/console.c
  src/ddc.c
  src/freertos_hook.c
  src/main.c
  src/nvs.c
  src/usb-cdc.c
  src/usb_descriptors.c
  src/usb-hid.c
  src/ws2812.c
  src/ws2812.pio)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated)

pico_generate_pio_header(${PROJECT} ${PROJECT_SOURCE_DIR}/src/ws2812.pio OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated)

target_include_directories(${PROJECT} PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/include/tusb
  ${PROJECT_SOURCE_DIR}/lib/cec_ulib/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated)

set(CEC_PIN "3" CACHE STRING "GPIO pin for HDMI CEC.")
set(PICO_CEC_VERSION "pico-cec" CACHE STRING "Pico-CEC version string.")
set(KEYMAP_DEFAULT "KODI" CACHE STRING "Default keymap, specify KODI or MISTER.")

set_source_files_properties(src/hdmi-cec.c PROPERTIES COMPILE_DEFINITIONS
  "CEC_PIN=${CEC_PIN}")

set_source_files_properties(src/usb-cdc.c PROPERTIES COMPILE_DEFINITIONS
  "PICO_CEC_VERSION=\"${PICO_CEC_VERSION}\"")

set_source_files_properties(src/console.c PROPERTIES COMPILE_DEFINITIONS
  "PICO_CEC_VERSION=\"${PICO_CEC_VERSION}\"")

# Undefine TinyUSB built-in OS, redefined in our tusb_config.h
target_compile_options(${PROJECT} PRIVATE
  -UCFG_TUSB_OS
  -DKEYMAP_DEFAULT_${KEYMAP_DEFAULT}=1)

target_link_libraries(${PROJECT}
  crc
  pico_stdlib
  pico_unique_id
  hardware_i2c
  hardware_pio
  tinyusb_device
  tinyusb_board
  FreeRTOS-Kernel
  tcli)

pico_add_extra_outputs(${PROJECT})
pico_set_binary_type(${PROJECT} copy_to_ram)
pico_set_linker_script(${PROJECT} ${PROJECT_SOURCE_DIR}/src/memmap_ram_nvs.ld)

# disable stdio and uart
pico_enable_stdio_usb(${PROJECT} 0)
pico_enable_stdio_uart(${PROJECT} 0)

#-------------------------------------------------------------------------------
# Create version header file for inclusion in the source code

# Dynamically determine project name from directory name
get_filename_component(my_project_name "${CMAKE_CURRENT_SOURCE_DIR}" NAME)

# Replace spaces with underscores
string(REPLACE " " "_" my_project_name "${my_project_name}")

# Set sanitized project name for the include file
set(PROJ_NAME "${my_project_name}")

set(PROJ_VER "34.42")

set(SDK_VERSION ${PICO_SDK_VERSION_STRING})

set(BOARD_NAME ${PICO_BOARD})

# Git info
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
    COMMAND bash -c "git diff --quiet || echo +dirty"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_DIRTY
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/project_info.h.in
    ${CMAKE_BINARY_DIR}/project_info.h
    @ONLY
)
