#include <stdio.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "tusb.h"

#include "hdmi-cec.h"

/* Intercept HDMI CEC commands, convert to a keypress and send to HID task
 * handler.
 *
 * Based (mostly ripped) from the Arduino version by Szymon Slupik:
 * https://github.com/SzymonSlupik/CEC-Tiny-Pro
 * which itself is based on the original code by Thomas Sowell:
 * https://github.com/tsowell/avr-hdmi-cec-volume/tree/master
 */

typedef struct {
  const char *name;
  uint8_t key;
} command_t;

/*
 * Key mapping from HDMI user control to HID keyboard entry.
 */
const command_t keymap[256] = {[0x00] = {"User Control Select", HID_KEY_ENTER},
                               [0x01] = {"User Control Up", HID_KEY_ARROW_UP},
                               [0x02] = {"User Control Down", HID_KEY_ARROW_DOWN},
                               [0x03] = {"User Control Left", HID_KEY_ARROW_LEFT},
                               [0x04] = {"User Control Right", HID_KEY_ARROW_RIGHT},
                               [0x0d] = {"User Control Exit", HID_KEY_BACKSPACE}};

/* The HDMI address for this device.  Respond to CEC sent to this address. */
#define ADDRESS 0x04
#define TYPE    0x04 // HDMI Playback 1

TaskHandle_t xCECTask;

typedef enum {
  GPIO_ACTION_NONE = 0x0,
  GPIO_ACTION_HIGH = 0x1,
  GPIO_ACTION_LOW = 0x2,
  GPIO_ACTION_GET = 0x3,
  GPIO_ACTION_MUST_LOW = 0x4
} gpio_action_t;

typedef struct {
  TaskHandle_t task;
  uint64_t start;
  uint64_t next;
  gpio_action_t action;
  bool state;
} gpio_alarm_t;

int64_t alarm_callback(alarm_id_t alarm, void *user_data) {
  gpio_alarm_t *ga = (gpio_alarm_t *)user_data;
  switch (ga->action) {
    case GPIO_ACTION_HIGH:
      gpio_set_dir(CECPIN, GPIO_IN);  // Set the CEC line back to high-Z.
      break;
    case GPIO_ACTION_LOW:
      gpio_set_dir(CECPIN, GPIO_OUT);  // Pull the CEC line low.
      break;
    case GPIO_ACTION_GET:
      ga->state = gpio_get(CECPIN);
      break;
    case GPIO_ACTION_MUST_LOW:
      gpio_set_dir(CECPIN, GPIO_IN);
      ga->action = GPIO_ACTION_NONE;
      return (ga->next - (time_us_64() - ga->start));
      break;
    case GPIO_ACTION_NONE:
    default:
  }
  xTaskNotifyFromISR(ga->task, 0, eNoAction, NULL);

  return 0;
}


static void send_ack(void) {
#if 0
  /* Send ACK.  Called immediately after a falling edge has occured. */
  uint64_t ticks_start;
  uint64_t ticks;

  ticks_start = time_us_64();

  if (ACTIVE)
    gpio_set_dir(CECPIN, GPIO_OUT);  // Pull the CEC line low.

  for (;;) {
    ticks = time_us_64();

    /* optimal 1.5 ms */
    if ((ticks - ticks_start) >= 1500) {
      gpio_set_dir(CECPIN, GPIO_IN);  // Set the CEC line back to high-Z.
      break;
    }
  }
#else
  uint64_t ticks_start = time_us_64();
  gpio_alarm_t ga = {
    .task = xCECTask,
    .action = GPIO_ACTION_HIGH};

  gpio_set_dir(CECPIN, GPIO_OUT);  // Pull the CEC line low.
  add_alarm_at(from_us_since_boot(ticks_start + 1500), alarm_callback, &ga, true);
  ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
#endif
}

volatile uint64_t edge_time;

void cec_edge_isr(uint gpio, uint32_t events) {
  edge_time = time_us_64();
  xTaskNotifyFromISR(xCECTask, 0, eNoAction, NULL);
}

#if 1
static uint64_t wait_edge(bool e) {
  edge_time = 0;

  if (e) {  // rising edge
    gpio_set_irq_enabled(CECPIN, GPIO_IRQ_EDGE_RISE, true);
  } else {  // falling edge
    gpio_set_irq_enabled(CECPIN, GPIO_IRQ_EDGE_FALL, true);
  }

  ulTaskNotifyTake(pdFALSE, portMAX_DELAY);

  gpio_set_irq_enabled(CECPIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

  return edge_time;
}
#else
static uint64_t wait_edge(bool e) {
  uint64_t ticks;
  uint64_t last, cec;

  last = cec = gpio_get(CECPIN);

  while (true) {
    ticks = time_us_64();

    last = cec;
    cec = gpio_get(CECPIN);

    if (e) { // rising edge
      if ((last == 0) && (cec == 1)) {
        return ticks;
      }
    } else { // falling edge
      if ((last == 1) && (cec == 0)) {
        return ticks;
      }
    }
  }
}
#endif

static uint64_t wait_falling_edge(void) {
  return wait_edge(0);
}

static uint64_t wait_rising_edge(void) {
  return wait_edge(1);
}

static uint8_t recv_data_bit(void) {
#if 1
  /* Sample a bit, called immediately after a falling edge occurs. */
  uint64_t ticks_start;
  uint64_t ticks;

  ticks_start = time_us_64();

  for (;;) {
    ticks = time_us_64();

    /* optimal 1.05 ms */
    if ((ticks - ticks_start) >= 1050) {
      return gpio_get(CECPIN);
    }
  }
#else
  uint64_t ticks_start = time_us_64();
  gpio_alarm_t ga = { .task = xCECTask, .action = GPIO_ACTION_GET, .state = false};

  add_alarm_at(from_us_since_boot(ticks_start + 1050), alarm_callback, &ga, true);
  ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
  return ga.state;
#endif
}

static void wait_start_bit(void) {
  /* A start bit consists of a falling edge followed by a rising edge
   * between 3.5 ms and 3.9 ms after the falling edge, and a second
   * falling edge between 4.3 and 4.7 ms after the first falling edge.
   * Wait until those conditions are met, and start over at the next
   * falling edge if any threshold is exceeded. */
  for (;;) {
    uint64_t ticks_start = wait_falling_edge();
    uint64_t ticks = wait_rising_edge();

    if ((ticks - ticks_start) >= 3900) {
      continue;  // Rising edge took longer than 3.9 ms
    }
    /* Rising edge occured between 3.5 ms and 3.9 ms */
    else if ((ticks - ticks_start) >= 3500) {
      ticks = wait_falling_edge();
      if ((ticks - ticks_start) >= 4700) {
        continue;  // Falling edge took longer than 4.7 ms
      }
      /* Falling edge between 4.3 ms and 4.7 ms means that
       * this has been a start bit! */
      else if ((ticks - ticks_start) >= 4300) {
        return;
      } else {
        continue;  // The falling edge came too early
      }
    } else {
      continue;  // The rising edge came sooner than 3.5 ms
    }
  }
}

static uint8_t recv_frame(uint8_t *pld, uint8_t address) {
  //gpio_put(PICO_DEFAULT_LED_PIN, true);
  uint64_t ticks_start;
  uint64_t ticks;
  uint8_t bit_count;
  uint8_t pldcnt;
  uint8_t eom;

  wait_start_bit();

  bit_count = 9;
  pldcnt = 0;
  pld[pldcnt] = 0;

  /* Read blocks into pld until the EOM bit signals that the message is
   * complete.  Each block is 10 bits consisting of information bits 7-0,
   * an EOM bit, and an ACK bit.  The initiator sends the information
   * bits and the EOM bit and expects the follower to send a '0' during
   * the ACK bit to acknowledge receipt of the block. */
  for (;;) {
    /* At this point in the loop, a falling edge has just occured,
     * either in wait_start_bit() above or wait_falling_edge() at
     * the end of the loop, so it is time to sample a bit. */
    ticks_start = time_us_64();

    /* Only store and return the information bits. */
    if (bit_count > 1) {
      pld[pldcnt] <<= 1;
      pld[pldcnt] |= recv_data_bit();
    } else {
      eom = recv_data_bit();
    }
    bit_count--;

    /* Wait for the starting falling edge of the next bit. */
    ticks = wait_falling_edge();
    if ((ticks - ticks_start) < 2050) {  // 2.05 ms
      printf("# frame aborted - too short");
      return -1;
    }
    ticks_start = ticks;
    /* If that was the EOM bit, it's time to send an ACK and either
     * return the data (if EOM was indicated by the initiator) or
     * prepare to read another block. */
    if (bit_count == 0) {
      /* Only ACK messages addressed to us  */
      if (((pld[0] & 0x0f) == address) || !(pld[0] & 0x0f)) {
        send_ack();
      }
      if (eom) {
        /* Don't consume the falling edge in this case
         * because it could be the start of the next
         * start bit! */
        return pldcnt + 1;
      } else {
        /* Wait for the starting falling edge of the next bit. */
        ticks = wait_falling_edge();
        if ((ticks - ticks_start) >= 2750) {  // 2.75 ms
          printf("# frame error - too long");
          return -1;
        }
      }
      bit_count = 9;
      pldcnt++;
      pld[pldcnt] = 0;
    }
  }
  //gpio_put(PICO_DEFAULT_LED_PIN, false);
}

void send_start_bit(void) {
  /* Pull the line low for 3.7 ms and then high again until the 4.5 ms
   * mark.  This function doesn't produce the final falling edge of the
   * start bit - that is left to send_data_bit(). */
  uint64_t ticks_start;

  ticks_start = time_us_64();

  //gpio_set_dir(CECPIN, GPIO_OUT);  // Pull the CEC line low.
  gpio_put(CECPIN, false);

#if 1
  uint64_t ticks;

  for (;;) {
    ticks = time_us_64();
    if ((ticks - ticks_start) >= 3700) {  // 3.7 ms
      break;
    }
  }

  //gpio_set_dir(CECPIN, GPIO_IN);  // Set the CEC line back to high-Z.
  gpio_put(CECPIN, true);

  for (;;) {
    ticks = time_us_64();
    if ((ticks - ticks_start) >= 4500) {  // 4.5 ms
      break;
    }
  }
#else
  gpio_alarm_t ga = {
    .task = xCECTask,
    .start = ticks_start,
    .next = 4500,
    .action = GPIO_ACTION_MUST_LOW};
  add_alarm_at(from_us_since_boot(ticks_start + 3700), alarm_callback, &ga, true);
  ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
#endif
}

void send_data_bit(int8_t bit) {
  /* A data bit consists of a falling edge at T=0ms, a rising edge, and
   * another falling edge at T=2.4ms.  The timing of the rising edge
   * determines the bit value.  The rising edge for an optimal logical 1
   * occurs at T=0.6ms.  The rising edge for an optimal logical 0 occurs
   * at T=1.5ms. */
  uint64_t ticks_start;
  uint64_t bit_time = bit ? 600 : 1500;  // 1 = 0.6ms, 0 = 1.5ms

  ticks_start = time_us_64();

  //gpio_set_dir(CECPIN, GPIO_OUT);  // Pull the CEC line low.
  gpio_put(CECPIN, false);

#if 1
  uint64_t ticks;
  for (;;) {
    ticks = time_us_64();

    if ((ticks - ticks_start) >= bit_time) {
      break;
    }
  }

  //gpio_set_dir(CECPIN, GPIO_IN);  // Set the CEC line back to high-Z.
  gpio_put(CECPIN, true);

  for (;;) {
    ticks = time_us_64();
    if ((ticks - ticks_start) >= 2400) {  // 2.4 ms
      break;
    }
  }
#else
  gpio_alarm_t ga = {
    .task = xCECTask,
    .start = ticks_start,
    .next = 2400,
    .action = GPIO_ACTION_MUST_LOW};
  add_alarm_at(from_us_since_boot(ticks_start + bit_time), alarm_callback, &ga, true);
  ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
#endif
}

static uint64_t time_next(uint64_t start, uint64_t next) {
  return (next - (time_us_64() - start));
}

int64_t hdmi_tx_callback(alarm_id_t alarm, void *user_data) {
  hdmi_frame_t *frame = (hdmi_frame_t *)user_data;

  uint64_t low_time = 0;
  switch (frame->state) {
    case HDMI_FRAME_STATE_START_LOW:
      gpio_set_dir(CECPIN, GPIO_OUT);
      //gpio_put(CECPIN, false);
      frame->start = time_us_64();
      frame->state = HDMI_FRAME_STATE_START_HIGH;
      return time_next(frame->start, 3700);
    case HDMI_FRAME_STATE_START_HIGH:
      gpio_set_dir(CECPIN, GPIO_IN);
      //gpio_put(CECPIN, true);
      frame->state = HDMI_FRAME_STATE_DATA_LOW;
      return time_next(frame->start, 4500);
    case HDMI_FRAME_STATE_DATA_LOW:
      gpio_set_dir(CECPIN, GPIO_OUT);
      //gpio_put(CECPIN, false);
      frame->start = time_us_64();
      low_time = (frame->message->data[frame->byte] & (1 << frame->bit)) ? 600 : 1500;
      frame->state = HDMI_FRAME_STATE_DATA_HIGH;
      return time_next(frame->start, low_time);
    case HDMI_FRAME_STATE_DATA_HIGH:
      gpio_set_dir(CECPIN, GPIO_IN);
      //gpio_put(CECPIN, true);
      if (frame->bit--) {
        frame->state = HDMI_FRAME_STATE_DATA_LOW;
      } else {
        frame->byte++;
        frame->state = HDMI_FRAME_STATE_EOM_LOW;
      }
      return time_next(frame->start, 2400);
    case HDMI_FRAME_STATE_EOM_LOW:
      gpio_set_dir(CECPIN, GPIO_OUT);
      //gpio_put(CECPIN, false);
      low_time = (frame->byte < frame->message->len) ? 1500 : 600;
      frame->start = time_us_64();
      frame->state = HDMI_FRAME_STATE_EOM_HIGH;
      return time_next(frame->start, low_time);
    case HDMI_FRAME_STATE_EOM_HIGH:
      gpio_set_dir(CECPIN, GPIO_IN);
      //gpio_put(CECPIN, true);
      frame->state = HDMI_FRAME_STATE_ACK_LOW;
      return time_next(frame->start, 2400);
    case HDMI_FRAME_STATE_ACK_LOW:
      gpio_set_dir(CECPIN, GPIO_OUT);
      //gpio_put(CECPIN, false);
      frame->start = time_us_64();
      frame->state = HDMI_FRAME_STATE_ACK_HIGH;
      return time_next(frame->start, 600);
    case HDMI_FRAME_STATE_ACK_HIGH:
      gpio_set_dir(CECPIN, GPIO_IN);
      //gpio_put(CECPIN, true);
      if (frame->byte < frame->message->len) {
        frame->bit = 7;
        frame->state = HDMI_FRAME_STATE_DATA_LOW;
        return time_next(frame->start, 2400);
      } else {
        frame->state = HDMI_FRAME_STATE_END;
      }
      // need to handle follower sending ack
      // fall through
    default:
      xTaskNotifyFromISR(xCECTask, 0, eNoAction, NULL);
      return 0;
  }
}

void hdmi_tx_frame(uint8_t *data, uint8_t len) {
  //gpio_put(PICO_DEFAULT_LED_PIN, true);

  // wait 7 bit times before sending
  vTaskDelay(pdMS_TO_TICKS(7 * 2.4));

  hdmi_message_t message = { data, len };
  hdmi_frame_t frame = {
    .message = &message,
    .bit = 7,
    .byte = 0,
    .start = 0,
    .state = HDMI_FRAME_STATE_START_LOW };
  add_alarm_at(from_us_since_boot(time_us_64()), hdmi_tx_callback, &frame, true);
  ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
  //gpio_put(PICO_DEFAULT_LED_PIN, false);
}

#if 1
static void send_frame(uint8_t pldcnt, uint8_t *pld) {
  gpio_put(PICO_DEFAULT_LED_PIN, true);
  hdmi_tx_frame(pld, pldcnt);
  gpio_put(PICO_DEFAULT_LED_PIN, false);
}
#else
static void send_frame(uint8_t pldcnt, uint8_t *pld) {
  gpio_put(PICO_DEFAULT_LED_PIN, true);
  uint8_t bit_count;
  uint8_t i;

  // wait 7 bit times before sending
  vTaskDelay(pdMS_TO_TICKS(7 * 2.4));
  send_start_bit();

  for (i = 0; i < pldcnt; i++) {
    bit_count = 7;
    /* Information bits. */
    do {
      send_data_bit((pld[i] >> bit_count) & 0x01);
    } while (bit_count--);
    /* EOM bit. */
    send_data_bit(i == (pldcnt - 1));
    /* ACK bit (we will assume the block was received). */
    send_data_bit(1);
  }
  gpio_put(PICO_DEFAULT_LED_PIN, false);
}
#endif

static void device_vendor_id(uint8_t initiator, uint8_t destination, uint32_t vendor_id) {
  uint8_t pld[5] = {(initiator << 4) | destination, 0x87, (vendor_id >> 16) & 0x0ff,
                    (vendor_id >> 8) & 0x0ff, (vendor_id >> 0) & 0x0ff};

  send_frame(5, pld);
  printf("\n<-- %02x:87 [Device Vendor ID]", pld[0]);
}

static void report_power_status(uint8_t initiator, uint8_t destination, uint8_t power_status) {
  uint8_t pld[3] = {(initiator << 4) | destination, 0x90, power_status};

  send_frame(3, pld);
  printf("\n<-- %02x:90 [Report Power Status]", pld[0]);
}

static void set_system_audio_mode(uint8_t initiator,
                                  uint8_t destination,
                                  uint8_t system_audio_mode) {
  uint8_t pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x72;
  pld[2] = system_audio_mode;

  send_frame(3, pld);
  printf("\n<-- %02x:72 [Set System Audio Mode]", pld[0]);
}

static void report_audio_status(uint8_t initiator, uint8_t destination, uint8_t audio_status) {
  uint8_t pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x7a;
  pld[2] = audio_status;

  send_frame(3, pld);
  printf("\n<-- %02x:7a [Report Audio Status]", pld[0]);
}

static void system_audio_mode_status(uint8_t initiator,
                                     uint8_t destination,
                                     uint8_t system_audio_mode_status) {
  uint8_t pld[3];

  pld[0] = (initiator << 4) | destination;
  pld[1] = 0x7e;
  pld[2] = system_audio_mode_status;

  send_frame(3, pld);
  printf("\n<-- %02x:7e [System Audio Mode Status]", pld[0]);
}

static void set_osd_name(uint8_t initiator, uint8_t destination) {
  gpio_put(PICO_DEFAULT_LED_PIN, true);
  uint8_t pld[6] = {(initiator << 4) | destination, 0x47, 'P', 'i', 'c', 'o'};

  send_frame(6, pld);
  printf("\n<-- %02x:47 [Set OSD Name]", pld[0]);
  gpio_put(PICO_DEFAULT_LED_PIN, false);
}

static void report_physical_address(uint8_t initiator,
                                    uint8_t destination,
                                    unsigned int physical_address,
                                    uint8_t device_type) {
  uint8_t pld[5] = {(initiator << 4) | destination, 0x84, (physical_address >> 8) & 0x0ff,
                    (physical_address >> 0) & 0x0ff, device_type};

  send_frame(5, pld);
  printf("\n<-- %02x:84 [Report Physical Address]", pld[0]);
}

void cec_task(void *data) {
  //QueueHandle_t *q = (QueueHandle_t *)data;
  bool active = false;

#if 0
  gpio_set_dir(CECPIN, GPIO_OUT);
  while (true) {
    device_vendor_id(ADDRESS, 0x0f, 0x0010FA);
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
#endif

  gpio_set_irq_enabled_with_callback(CECPIN, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true, &cec_edge_isr);
  gpio_set_irq_enabled(CECPIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);

  while (true) {
    uint8_t pld[16];
    uint8_t pldcnt, pldcntrcvd;
    uint8_t initiator, destination;
    uint8_t i;
    //uint8_t key = HID_KEY_NONE;

    pldcnt = recv_frame(pld, ADDRESS);
    pldcntrcvd = pldcnt;
    if (pldcnt < 0) {
      printf("%i\n", pldcnt);
      return;
    }
    initiator = (pld[0] & 0xf0) >> 4;
    destination = pld[0] & 0x0f;

    printf("%s", "--> ");
    for (i = 0; i < pldcnt - 1; i++) {
      printf("%02x:", pld[i]);
    }
    printf("%02x ", pld[i]);

    if (!active) {
      report_physical_address(ADDRESS, 0x0f, 0x1000, TYPE);
      device_vendor_id(ADDRESS, 0x0f, 0x0010FA);
    }

    if ((pldcnt > 1)) {
      switch (pld[1]) {
        case 0x04:
          printf("[Image View On]");
          break;
        case 0x0d:
          printf("[Text View On]");
          break;
        case 0x36:
          printf("[Standby]");
          printf("<*> [Turn the display OFF]");
          active = false;
          break;
        case 0x70:
          printf("[System Audio Mode Request]");
          if (ACTIVE && (destination == ADDRESS))
            set_system_audio_mode(ADDRESS, 0x0f, 1);
          break;
        case 0x71:
          printf("[Give Audio Status]");
          if (ACTIVE && (destination == ADDRESS))
            report_audio_status(ADDRESS, initiator, 0x32);  // volume 50%, mute off
          break;
        case 0x72:
          printf("[Set System Audio Mode]");
          break;
        case 0x7d:
          printf("[Give System Audio Mode Status]");
          if (ACTIVE && (destination == ADDRESS))
            system_audio_mode_status(ADDRESS, initiator, 1);
          break;
        case 0x7e:
          printf("[System Audio Mode Status]");
          break;
        case 0x82:
          printf("[Active Source]");
          printf("<*> [Turn the display ON]");
          break;
        case 0x84:
          printf("[Report Physical Address>]");
          break;
        case 0x85:
          printf("[Request Active Source]");
          break;
        case 0x87:
          printf("[Device Vendor ID]");
          break;
        case 0x8c:
          printf("[Give Device Vendor ID]");
          if (ACTIVE && (destination == ADDRESS))
            device_vendor_id(ADDRESS, 0x0f, 0x0010FA);
          break;
        case 0x8e:
          printf("[Menu Status]");
          break;
        case 0x8f:
          printf("[Give Device Power Status]");
          if (ACTIVE && (destination == ADDRESS))
            report_power_status(ADDRESS, initiator, 0x00);
          /* Hack for Google Chromecast to force it sending V+/V- if no CEC TV is present */
          if (ACTIVE && (destination == 0))
            report_power_status(0, initiator, 0x00);
          break;
        case 0x90:
          printf("[Report Power Status]");
          break;
        case 0x91:
          printf("[Get Menu Language]");
          break;
        case 0x9d:
          printf("[Inactive Source]");
          break;
        case 0x9e:
          printf("[CEC Version]");
          break;
        case 0x9f:
          printf("[Get CEC Version]");
          break;
        case 0x46:
          printf("[Give OSD Name]");
          if (ACTIVE && (destination == ADDRESS))
            set_osd_name(ADDRESS, initiator);
          break;
        case 0x47:
          printf("[Set OSD Name]");
          active = true;
          break;
        case 0x83:
          printf("[Give Physical Address]");
          if (ACTIVE && (destination == ADDRESS))
            report_physical_address(ADDRESS, 0x0f, 0x1000, TYPE);
          break;
        case 0x44:
          switch (pld[2]) {
            case 0x41:
              printf("[User Control Volume Up]");
              break;
            case 0x42:
              printf("[User Control Volume Down]");
              break;
            default:
              command_t command = keymap[pld[2]];
              if (command.name != NULL) {
                printf(command.name);
                //xQueueSend(*q, &command.key, pdMS_TO_TICKS(10));
              }
              break;
          }
          break;
        case 0x45:
          printf("[User Control Released]");
          //key = HID_KEY_NONE;
          //xQueueSend(*q, &key, pdMS_TO_TICKS(10));
          break;
        case 0xFF:
          printf("[Abort]");
          active = false;
        default:
          if (pldcntrcvd > 1)
            printf("???");  // undecoded command
          break;
      }
    } else
      printf("[Ping]");
    printf("\n");
  }
}
