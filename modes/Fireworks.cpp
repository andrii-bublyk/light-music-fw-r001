#include "Fireworks.h"

Fireworks::Fireworks(WS2812B* pixels)
{
    this->current_node = 0;
    this->pixels = pixels;
}

void Fireworks::run()
{
    current_node++;
    if (this->current_node == 50)
    {
        this->current_node = 5;
    }
    uint32_t current_color = 0;
    for (int i = 0; i < 120; i++)
	{
        if (i == this->current_node)
        {
            current_color = this->adjust_brightness(this->pixels->Color(200, 0, 0), 0.8);
        }
        else
        {
            current_color = this->adjust_brightness(this->pixels->Color(0, 100, 0), 0.8);
        }
		this->pixels->setPixelColor(i, current_color);
	}
}

uint32_t Fireworks::adjust_brightness(uint32_t c, float amt)
{
	// pull the R,G,B components out of the 32bit color value
	uint8_t r = (uint8_t)(c >> 16);
	uint8_t g = (uint8_t)(c >> 8);
	uint8_t b = (uint8_t)c;
	// Scale
	r = r * amt;
	g = g * amt;
	b = b * amt;
	// Pack them into a 32bit color value again.
	return pixels->Color(r, g, b);
}