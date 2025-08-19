idf_component_register(SRCS
                    "app_main.c"
                    "../src/blink.c"
                    "../src/config-keymap.c"
                    "../src/console.c"
                    "../src/ddc.c"
                    "../src/debug.c"
                    "../src/freertos_hook.c"
                    "../src/main.c"
                    "../src/nvs.c"
                    "../src/usb_descriptors_esp.c"
                    "../src/usb-init.c"
                    "../src/usb-hid.c"
                    "../src/usb-cdc.c"
                    "../tcli/source/tcli.c"
                    "../tcli/source/tclie.c"
                    "../lib/cec_ulib/cec-cmd.c"
                    "../lib/cec_ulib/cec-config.c"
                    "../lib/cec_ulib/cec-frame.c"
                    "../lib/cec_ulib/cec-hal.c"
                    "../lib/cec_ulib/cec-id.c"
                    "../lib/cec_ulib/cec-log.c"
                    "../lib/cec_ulib/cec-task.c"
                    "../lib/cec_ulib/cec-user.c"
                    "../lib/cec_ulib/cec-util.c"
                    "../lib/esp_idf/esp-idf.c"
                    "../lib/esp_idf/prefs.c"
                    "../lib/esp_idf/crc/crc32.c"
                    PRIV_REQUIRES spi_flash esp_timer nvs_flash
                    REQUIRES driver
                    PRIV_INCLUDE_DIRS
                    "${CMAKE_BINARY_DIR}"
                    INCLUDE_DIRS "."
                    "../include"
                    "../tcli/include"
                    "../lib/cec_ulib/include"
                    "../lib/esp_idf"
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
