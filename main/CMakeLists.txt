idf_component_register(SRCS
                    "app_main.c"
                    "../src/blink.c"
                    "../src/cec-config.c"
                    "../src/cec-log.c"
                    "../src/debug.c"
                    "../src/freertos_hook.c"
                    "../src/hdmi-cec.c"
                    "../src/hdmi-ddc.c"
                    "../src/main.c"
                    "../src/nvs.c"
#                    "../src/usb_descriptors.c"
                    "../src/usb_hid.c"
                    "../src/usb-cdc.c"
#                    "../src/ws2812.c"
#                    "../src/ws2812.pio"
                    "../tcli/source/tcli.c"
                    "../tcli/source/tclie.c"
                    "../lib/espidf/gpio.c"
                    "../lib/espidf/portable.c"
                    "../lib/espidf/crc/crc32.c"
                    PRIV_REQUIRES spi_flash esp_timer
                    REQUIRES driver
                    INCLUDE_DIRS
                    "../include"
                    "../tcli/include"
                    "../lib/espidf"
                    )

add_definitions(
  -DTCLI_COMPLETE=1
  -DTCLIE_ENABLE_USERS=0
  -DTCLIE_PATTERN_MATCH=1
  -Wno-implicit-fallthrough
  -Wno-missing-field-initializers
  )
