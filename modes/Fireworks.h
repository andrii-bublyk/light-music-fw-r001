#ifndef FIREWORKS_H
#define FIREWORKS_H

#include <Arduino.h>
#include <WS2812B.h>
#include <SPI.h>

class Fireworks {
public:
    Fireworks(WS2812B* pixels);
    void run();
private:
    WS2812B* pixels;
    uint8_t current_node;
    uint32_t adjust_brightness(uint32_t c, float amt);
};

#endif // FIREWORKS_H