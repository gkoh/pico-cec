// From LaserBearIndustries 2025
#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>

void ws2812_init(unsigned int pin);
void ws2812_put_rgb(uint8_t red, uint8_t green, uint8_t blue);
void ws2812_put_pixel(uint32_t pixel_grb);

#endif  // WS2812_H
