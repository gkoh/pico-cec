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
* CEC_PHYS_ADDR: specify physical address, defaults to 0x1000 (1.0.0.0, HDMI input 1)

Example invocation to specify:
* use Raspberry Pi Pico development board
* use GPIO pin 11
* use physical address 0x1100 (1.1.0.0, HDMI input 1 and input 1 of that device)

```
$ cmake -DPICO_BOARD=pico -DCEC_PIN=11 -DCEC_PHYS_ADDR=0x1100 ..
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

# Real World Usage
This is currently working with:
* a Sharp 60" TV (physical address 0x1000)
   * directly connected to TV HDMI input 1
* through a Denon AVR connected to the Sharp TV (physical address 0x1100)
   * Pico-CEC connected to Denon AVR HDMI input 1
   * Denon AVR connected to TV HDMI input 1

# Design
## Hardware
The hardware connections are extremely simple. Both HDMI CEC and the Pico are
3.3V obviating the need for level shifters.

Additionally, we rely on the GPIO input/output impedance states to read or drive
the CEC bus. In doing so we only need 2 wires:
* HDMI CEC pin 13 direct to a GPIO
* HDMI CEC ground pin 17 direct to GND

For the Seeed Studio XIAO RP2040:
* HDMI pin 13 --> D10
* HDMI pin 17 --> GND

After this we:
* connect `Pico-CEC` to the HDMI output of the PC
* connect the HDMI cable from the TV to `Pico-CEC`
* connect a USB cable from `Pico-CEC` to the PC

### Prototype
![Initial prototype.](https://github.com/gkoh/pico-cec/assets/5484552/ca15af77-d33d-41e0-9339-2782b908115f)

### Assembly
![XIAO RP2040 and HDMI pass through.](https://github.com/gkoh/pico-cec/assets/5484552/40f6f3b6-7869-4254-b7a0-7f342fdb7ce0)

![Partially assembled Pico-CEC.](https://github.com/gkoh/pico-cec/assets/5484552/13374ca2-a17a-4fcc-a04c-6e94110662ed)

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
* `recv_frame`
   * receives and validates CEC packets from the CEC GPIO pin
   * edge interrupt driven state machine
      * rewritten from busy wait loop to reduce CPU load
* `send_frame`
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
* FreeRTOS
* pico-sdk
   * tinyusb

# Hardware
* Seeed Studio XIAO RP2040 (chosen for form factor)
   * https://www.seeedstudio.com/XIAO-RP2040-v1-0-p-5026.html
   * Originally prototyped on the Raspberry Pi Pico board (still works with
     minor build time tweaks)
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
| Random short wires | 2 | basically free |
| Scunge 3D print from friend | 1 | mostly free |
| Umpteen hours of engineering | 1 | priceless |
| Total | 16.13 |

## Future
* physical address is currently hard coded
   * can curently be specified by build flags
   * ideally, tap pin 16 and sniff the physical address from the EDID traffic
      * research says this is 'just' i2c, needs further investigation and
        testing
      * EDID is 5V, Pico GPIO _might_ be 5V tolerant, may need level shifter
* logical address is hardcoded to 4 (Player 1)
   * make it a CMake variable ala physical address
   * perform a CEC ping and choose a non-occupied logical address
* more blinken LEDs
   * most Pico boards appear to have an RGB LED, use it
   * perhaps:
      * R - FreeRTOS crash handlers?
      * G - HDMI message received?
      * B - HID keyboard message sent?
* pass cec-compliance
   * https://docs.kernel.org/userspace-api/media/cec/cec-intro.html
* survive cec-compliance fuzzing

# References
Inspiration and/or ground work was obtained from the following:
* https://github.com/SzymonSlupik/CEC-Tiny-Pro
* https://github.com/tsowell/avr-hdmi-cec-volume
