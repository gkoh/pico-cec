idf_component_register(SRCS
                    "app_main.c"
                    "../src/blink.c"
                    "../src/cec-cmd.c"
                    "../src/cec-config.c"
                    "../src/cec-frame.c"
                    "../src/cec-log.c"
                    "../src/cec-task.c"
                    "../src/cec-user.c"
                    "../src/cec-util.c"
                    "../src/ddc.c"
                    "../src/debug.c"
                    "../src/freertos_hook.c"
                    "../src/main.c"
                    "../src/nvs.c"
#                    "../src/usb_descriptors.c"
                    "../src/usb_hid.c"
                    "../src/usb-cdc.c"
#                    "../src/ws2812.c"
#                    "../src/ws2812.pio"
                    "../tcli/source/tcli.c"
                    "../tcli/source/tclie.c"
                    "../lib/espidf/esp-port.c"
                    "../lib/espidf/portable.c"
                    "../lib/espidf/prefs.c"
                    "../lib/espidf/crc/crc32.c"
                    PRIV_REQUIRES spi_flash esp_timer nvs_flash
                    REQUIRES driver
                    PRIV_INCLUDE_DIRS
                    "${CMAKE_BINARY_DIR}"
                    INCLUDE_DIRS "."
                    "../include"
                    "../tcli/include"
                    "../lib/espidf"
                    )

# Append the FreeRTOS kernel's internal include directory
set(FREERTOS_KERNEL_INCLUDE "${IDF_PATH}/components/freertos/FreeRTOS-Kernel/include/freertos")
target_include_directories(${COMPONENT_LIB} PRIVATE "${FREERTOS_KERNEL_INCLUDE}")

add_definitions(
  -DDEBUG
  -DCEC_PIN=4
  -DSTACK_WORDSIZE=8
  -DTCLI_COMPLETE=1
  -DTCLIE_ENABLE_USERS=0
  -DTCLIE_PATTERN_MATCH=1
  -Wno-implicit-fallthrough
  -Wno-missing-field-initializers
  )
