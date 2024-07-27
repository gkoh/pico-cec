# debug output, no USB, just prints to serial
set(PROJECT_DEBUG ${PROJECT}-debug)
add_executable(${PROJECT_DEBUG}
  src/freertos_hook.c
  src/hdmi-cec.c
  src/hdmi-ddc.c
  src/debug.c)

target_include_directories(${PROJECT_DEBUG} PRIVATE
  ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_DEBUG}
  pico_stdlib
  pico_unique_id
  hardware_i2c
  FreeRTOS-Kernel)

pico_add_extra_outputs(${PROJECT_DEBUG})
pico_set_binary_type(${PROJECT_DEBUG} copy_to_ram)

# enable stdio
pico_enable_stdio_usb(${PROJECT_DEBUG} 1)
