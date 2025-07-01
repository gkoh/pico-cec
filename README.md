# Pico-CEC

A Raspberry Pi Pico based project to bridge HDMI CEC (Consumer Electronics
Control) and USB HID keyboard control (especially for use with Kodi).

![Fully assembled Pico-CEC.](https://github.com/gkoh/pico-cec/assets/5484552/443a47cb-7011-49a5-afbe-c527a2d5b086)

## Motivation

Micro/mini desktops are plentiful as second hand, budget friendly media players,
especially when installed with Kodi (eg. LibreELEC).
However, many of these devices do not support HDMI-CEC and require the user to
use additional peripherals (eg. wireless keyboard, game controller).

In this project we use a Pico to both:
* handle the CEC protocol on the HDMI port
* adapt the user control messages into USB keyboard inputs

## What Works
* HDMI CEC frame send and receive
* EDID parsing to determine HDMI physical address
* LibreELEC recognises Pico-CEC as an USB HID keyboard
* HDMI CEC basic user control messages are properly mapped to Kodi shortcuts,
  including:
   * navigations arrows
   * select
   * back
   * play
   * pause
   * numbers 0-9

## Cloning
To avoid cloning unneeded code, clone like this:
```
git -c submodule.active="lib/tinyusb" -c submodule.active=":(exclude,glob){lib,hw}/*" clone --recurse-submodules
```

Alternatively, clone everything in pico-sdk and tinyUSB:
```
git clone --recurse-submodules
```

## Building
This project uses the 'normal' CMake based build. To build the default image:
```
$ git clone <blah blah as above>
$ cd pico-cec
$ mkdir build && cd build
$ cmake ..
$ make
```

### Customising the Build
The CMake project supports three options:
* PICO_BOARD: specify variant of Pico board, defaults to Seeed XIAO RP2040
* CEC_PIN: specify GPIO pin for HDMI CEC, defaults to GPIO3

Example invocation to specify:
* use Raspberry Pi Pico development board
* use GPIO pin 11

```
$ cmake -DPICO_BOARD=pico -DCEC_PIN=11 ..
$ make
```

## Installing
Assuming a successful build, the build directory will contain `pico-cec.uf2`,
this can be written to the Pico as per normal:
* connect the Pico to computer via USB cable
* reset the Pico by holding 'Boot' and pressing 'Reset'
   * Pico now presents as a USB mass storage device
* copy `pico-cec.uf2` to the Pico
* disconnect

A command line interface over serial port is available, the guide can be found
here: [Command Line Interface Guide](https://github.com/gkoh/pico-cec/wiki/Command-Line-Interface-Guide)

## Blinking Lights
The RGB LED provides basic functional diagnosis:
* blue 2Hz: idle, CEC standby
* green 2Hz: CEC active
* green flash: CEC user button pressed
* red: crash

If there are no lights, something is very wrong.
If this occurs, please consider raising an issue.

# Real World Usage
This is currently working with:
* a Sharp 60" TV (physical address 0x1000)
   * directly connected to TV HDMI input 1
* through a Denon AVR connected to the Sharp TV (physical address 0x1100)
   * Pico-CEC connected to Denon AVR HDMI input 1
   * Denon AVR connected to TV HDMI input 1
* a Sony 60" TV (physical address 0x1000)
   * directly connected to TV HDMI input 1

# Design
## Hardware
The hardware connections are extremely simple. Both HDMI CEC and the Pico are
3.3V obviating the need for level shifters. The DDC bus (for EDID) is I2C and
5V, however, the Pico appears to be 5V tolerant.

Additionally, we rely on the GPIO input/output impedance states to read or drive
the CEC bus. DDC is I2C requiring data and clock lines.
Thus, we need to directly connect four wires:
* HDMI CEC pin 13 direct to a GPIO
* HDMI CEC ground pin 17 direct to GND
* HDMI DDC clock pin 15 direct to SCL
* HDMI DDC data pin 16 direct to SDA

For the Seeed Studio XIAO RP2040:
* HDMI pin 13 --> D10
* HDMI pin 17 --> GND
* HDMI pin 15 --> D5
* HDMI pin 16 --> D4

### Schematic
![Basic schematic.](https://github.com/user-attachments/assets/e7cff86f-3934-4b47-be95-2e42a42eb1ed)

After this we:
* connect `Pico-CEC` to the HDMI output of the PC
* connect the HDMI cable from the TV to `Pico-CEC`
* connect a USB cable from `Pico-CEC` to the PC

### Prototype

![Initial prototype.](https://github.com/user-attachments/assets/88f2631f-e33f-4994-91dc-cc9e3c07016a)

### Enclosure
The enclosure is a reasonably simple three piece sandwich 3d print modelled with OpenSCAD. It is designed to be printed as three separate pieces which are bolted together with M3 nuts and bolts.
An exploded preview of the result can be found in this [STL](openscad/pico-cec.stl).


### Assembly
![XIAO RP2040 with HDMI pass through and DDC.](https://github.com/user-attachments/assets/01c244b4-b5af-4926-94d2-38306876485b)

![Partially assembled Pico-CEC.](https://github.com/user-attachments/assets/c37bb127-409a-4ed1-acc1-4e83cf8a6d58)

## Software
The software is extremely simple and built on FreeRTOS tasks:
* cec_task
   * interact with HDMI CEC sending user control message inputs to a queue
* hid_task
   * read the user control messages from the queue and send to the USB task
* usbd_task
   * generate an HID keyboard input for the USB host
* blink_task
   * heart beat, no blink == no work

## cec_task
The CEC task comprises three major components:
* `cec_frame_recv`
   * receives and validates CEC packets from the CEC GPIO pin
   * edge interrupt driven state machine
      * rewritten from busy wait loop to reduce CPU load
* `cec_frame_send`
   * formats and sends CEC packets on the CEC GPIO pin
   * alarm interrupt driven state machine
      * rewritten from busy wait loop to reduce CPU load
* main control loop
   * manages CEC send and receive

All the HDMI frame handling was rewritten to be hardware/timer interrupt driven
to meet real-time constraints.
Attempts to increase the FreeRTOS tick timer along with busy wait loops were
simply unable to consistently meet the CEC timing windows.

## hid_task and usbd_task

These are simple FreeRTOS tasks effectively taken straight from the TinyUSB
examples.

## Dependencies
This project uses:
* [crc](https://github.com/gityf/crc)
* [FreeRTOS-Kernel](https://github.com/FreeRTOS/FreeRTOS-Kernel)
* [pico-sdk](https://github.com/raspberrypi/pico-sdk)
   * [tinyusb](https://github.com/hathach/tinyusb)
* [tcli](https://github.com/dpse/tcli)

# Hardware
* Seeed Studio XIAO RP2040 (chosen for form factor)
   * https://www.seeedstudio.com/XIAO-RP2040-v1-0-p-5026.html
   * Originally prototyped on the Raspberry Pi Pico board (still works but
     requires RGB unhacking)
* HDMI male/female passthrough adapter
   * Listed as 'HDMI Male and Female Test Board MINI Connector with Board PCB
     2.54mm pitch 19/20pin DP HD A Female To Male Adapter Board'
   * Model number: WP-905
   * https://www.aliexpress.com/item/1005004791079117.html
* custom 3d printed housing

# Bill of Materials
| Component | Quantity | Price (June 2024) (AUD) |
| :--- | ---: | ---: |
| Seeed Studio XIAO RP2040 | 1 | 11.50 |
| HDMI male/female adapter | 1 | 4.30 |
| M3x10mm bolt & nut | 2 | 0.16 |
| M3x20mm bolt & nut | 2 | 0.17 |
| Random short wires | 4 | basically free |
| Scunge 3D print from friend | 1 | mostly free |
| Umpteen hours of engineering | 1 | priceless |
| Total | | 16.13 |

# cec-compliance
As of v0.2.2, `pico-cec` now passes the cec-compliance test suite found in the
Linux `v4l-utils` package.
More details can be found in the wiki entry:
https://github.com/gkoh/pico-cec/wiki/CEC-Compliance-Testing

Furthermore, `pico-cec` has been able to survive one hour of cec-compliance fuzz
testing.

# Debugging
A command line terminal over serial port is supported.
Details can be found in the wiki entry:
https://github.com/gkoh/pico-cec/wiki/Command-Line-Interface-Guide

In particular, `debug on` will log all CEC traffic to the terminal.

# Future
* implement CEC send and receive in PIO
* port to ESP32?
   * WS2812 driver will need platform support, perhaps to RMT
   * implement CEC in RMT

# References
Inspiration and/or ground work was obtained from the following:
* https://github.com/SzymonSlupik/CEC-Tiny-Pro
* https://github.com/tsowell/avr-hdmi-cec-volume
