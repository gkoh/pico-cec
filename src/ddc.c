#include <stdio.h>
#include <string.h>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

#include "cec-log.h"
#include "ddc.h"

#define EDID_BLOCK_SIZE (128)
#define EDID_I2C_TIMEOUT_US (100 * 1000)
#define EDID_I2C_ADDR (0x50)
#define EDID_I2C_READ_SIZE (EDID_BLOCK_SIZE * 2)
#define EDID_CTA_DTD_START (0x02)
#define EDID_CTA_DBC_OFFSET (0x04)

#define I2C_MASTER_FREQUENCY (100 * 1000)

const uint8_t header[8] = {0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
const uint8_t ctahdr[2] = {0x02, 0x03};
const uint8_t vsbhdr[3] = {0x03, 0x0c, 0x00};

static void ddc_init() {
  i2c_init(i2c_default, I2C_MASTER_FREQUENCY);
  gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
  gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
}

static void ddc_exit() {
  i2c_deinit(i2c_default);
}

/**
 * Calculate and verify EDID checksum.
 */
static int verify(uint8_t *edid, size_t len) {
  uint16_t cksum = 0x0000;

  // log the data
  if ((len % 8) == 0) {
    for (size_t i = 0; i < len; i += 8) {
      cec_log_submitf("[%d] %02x %02x %02x %02x %02x %02x %02x %02x"_LOG_BR, i, edid[i],
                      edid[i + 1], edid[i + 2], edid[i + 3], edid[i + 4], edid[i + 5], edid[i + 6],
                      edid[i + 7]);
    }
  } else {
    for (size_t i = 0; i < len; i++) {
      cec_log_submitf("[%d] %02x"_LOG_BR, i, edid[i]);
    }
  }

  // calculate the checksum
  for (size_t i = 0; i < len; i++) {
    cksum += edid[i];
  }

  return ((cksum & 0x00ff) == 0x00 ? PICO_ERROR_NONE : PICO_ERROR_GENERIC);
}

/**
 * Read a 256 byte block and verify the EDID block checksums.
 */
static int read_edid_block(uint8_t *edid, size_t len) {
  int ret = i2c_read_timeout_us(i2c_default, EDID_I2C_ADDR, edid, len, false, EDID_I2C_TIMEOUT_US);
  if (ret != len) {
    cec_log_submitf("Failed to read %d bytes from 0x%02x"_LOG_BR, len, EDID_I2C_ADDR);
    return PICO_ERROR_GENERIC;
  }

  if (verify(edid, len)) {
    cec_log_submitf("Failed to verify EDID block checksum"_LOG_BR);
    return PICO_ERROR_GENERIC;
  }

  cec_log_submitf("Read %d bytes from 0x%02x"_LOG_BR, ret, EDID_I2C_ADDR);

  return PICO_ERROR_NONE;
}

/**
 * Parse a data block and return the physical address if found.
 *
 * Returns 0x0000 if the block is not a vendor specific data block or the
 * physical address is not found.
 */
static uint16_t find_physical_address(uint8_t *block, size_t len) {
  if (len < 4) {
    // Too short for Vendor Specific Data Block
    return 0x0000;
  }

  if (memcmp(&block[1], vsbhdr, 3) == 0) {
    // HDMI Licensing, LLC block
    uint16_t addr = (block[4] << 8) | block[3];
    cec_log_submitf("  physical address = %04x"_LOG_BR, addr);
    return addr;
  }

  return 0x0000;
}

static uint16_t get_physical_address(void) {
  uint8_t edid[EDID_I2C_READ_SIZE] = {0};

  if (read_edid_block(edid, EDID_I2C_READ_SIZE)) {
    return 0x0000;
  }

  if (memcmp(edid, header, 8)) {
    // not an EDID block
    return 0x0000;
  }

  cec_log_submitf(" EDID header"_LOG_BR);
  if (edid[126] == 0x00) {
    cec_log_submitf("Missing CTA extensions"_LOG_BR);
    return 0x0000;
  }

  uint8_t *cta = &edid[EDID_BLOCK_SIZE];
  if (memcmp(cta, ctahdr, 2) == 0) {
    // Valid CTA extension block
    cec_log_submitf(" CTA Extension"_LOG_BR);
    cec_log_submitf("    DTD start: 0x%02x"_LOG_BR, cta[EDID_CTA_DTD_START]);

    uint8_t offset = EDID_CTA_DBC_OFFSET;
    for (uint8_t i = offset; i < cta[EDID_CTA_DTD_START];) {
      uint8_t *db = &cta[i];
      uint8_t len = (db[0] & 0x1f);
      cec_log_submitf("  [%u](%u) data block: %02x"_LOG_BR, i, len, db[0]);
      if (len == 0x00) {
        i++;
        continue;
      }

      uint16_t addr = find_physical_address(db, len);
      if (addr != 0x0000) {
        return addr;
      }

      i += len + 1;  // payload + header
    }
  }

  return 0x0000;
}

uint16_t ddc_get_physical_address(void) {
  uint8_t zero = 0x00;
  uint16_t address = 0x0000;

  ddc_init();

  cec_log_submitf("%s"_LOG_BR, "Issuing DDC reset");
  // issue a DDC reset
  int ret = i2c_write_timeout_us(i2c_default, EDID_I2C_ADDR, &zero, 1, true, EDID_I2C_TIMEOUT_US);
  if (ret != 1) {
    cec_log_submitf("Failed to write DDC reset: %s"_LOG_BR,
                    ret == PICO_ERROR_TIMEOUT ? "timeout" : "generic");
    return 0x0000;
  }

  address = get_physical_address();
  ddc_exit();

  return address;
}
