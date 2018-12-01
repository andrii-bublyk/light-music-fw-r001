// r006.1

//FFT stuff------------------------------------------------------------------
#define FFTLEN 1024
#include "cr4_fft_1024_stm32.h"
#include <SPI.h>
uint16_t data16[FFTLEN];
uint32_t data32[FFTLEN];
uint32_t y[FFTLEN];
//LED stuff-----------------------------------------------------------------
#include <WS2812B.h> //#include <NeoMaple.h>

#define NUMPIXELS 90 // number leds of the LED strip
#define NUM_REPEAT 1 // number of repeated times

WS2812B pixels = WS2812B(NUMPIXELS * NUM_REPEAT);

//DMA--------------------------------------------------------------------------
volatile static bool dma1_ch1_Active;
#include <libmaple/pwr.h>
#include <libmaple/scb.h>
#include <libmaple/rcc.h>
#include <libmaple/adc.h>

// modes------------------------------------------------------------------------
// #include "modes/Fireworks.h"

//Configurable_parameters------------------------------------------------------
int bins = 10; //max = 240

uint32_t vol_threshold = 55; //remove the noise
float low_freq_threshold = 0.15;
float high_freq_threshold = 3.5;

// pattern = 1: stars
// pattern = 2: power
// pattern = 3: CandleJars
// pattern = 4: Fireworks
// pattern = 5: Liquid mode 0
// pattern = 6: Liquid mode 1
// pattern = 7: Liquid mode 2
uint8_t pattern = 1;

//For pattern 1
int p2_vol_threshold_1 = 70;  //remove the noise
float p2_coeff_smooth = 0.8;  // 0 to 1.0
uint8_t p1_coeff_fading_light = 30;  // 30*37 = 1100ms
float p1_coeff_smooth = 20;
//Other stuff------------------------------------------------------------------
// float current[NUMPIXELS];
float brightness[NUMPIXELS];
// uint8_t k[NUMPIXELS];
uint8_t fadeK[NUMPIXELS];
uint32_t strip[NUMPIXELS];

const int8_t analogInPin = 1;

//Buttons------------------------------------------------------------------
const int8_t BTN_MODE_PIN = 15;  // PA15
const int8_t BTN_COLOR_PIN = 24; // PB8
const int8_t BTN_POWER_PIN = 30; // PB14

bool btnModePrevState = false;
bool btnColorPrevState = false;
bool btnPowerPrevState = false;

bool workEnable = true;
const int8_t EMBEDDED_COLORS_NUMBER = 3;
int8_t colorIndex = 0;
const int8_t LIQUID_WAVE_COLORS_SCHEMA[EMBEDDED_COLORS_NUMBER][4] = {
	{200, 56, 0, 50},
	{100, 100, 0, 80},
	{0, 200, 50, 60}};

const int8_t LIQUID_STATIC_COLORS_SCHEMA[EMBEDDED_COLORS_NUMBER][4] = {
	{25, 100, 100, 10},
	{40, 100, 0, 20},
	{0, 0, 200, 10}};
//Sound aligning-----------------------------------------------------------
const uint8_t MAX_VOLUME_FIND_ITERATION = 50;
uint8_t maxValueForTheLastPeriod = 1;
float volumeMultiplier = 1.0;

class Fireworks
{
  public:
	uint8_t const BACKGROUND_COLOR_R = 20;
	uint8_t const BACKGROUND_COLOR_G = 0;
	uint8_t const BACKGROUND_COLOR_B = 0;

	uint8_t const FIRE_WAVE_LENGTH = 15;
	uint8_t const FIRE_COLOR_R = 200;
	uint8_t const FIRE_COLOR_G = 0;
	uint8_t const FIRE_COLOR_B = 0;
	uint8_t const FIRE_ATTENUATION_ITERATIONS_NUMBER = 30;

	uint8_t const STAR_COLOR_R = 200;
	uint8_t const STAR_COLOR_G = 200;
	uint8_t const STAR_COLOR_B = 200;
	uint8_t const STAR_COLOR_BRIGHTNESS = 100;
	uint8_t const STAR_COUNT = 7;
	uint8_t const STAR_LIGHTING_ITERATIONS_COUNT = 20;

	uint8_t const NOIZE_THRESHOLD = 50;			 // [0..100]
	uint8_t const LAST_FOURIER_VALID_INDEX = 24; // [0..120]

	Fireworks(WS2812B *pixels)
	{
		this->pixels = pixels;
		current_state = 0;
		current_node = 1;
		fire_iteration = 0;
		state_finished = false;
		fire_attenuation_counter = 0;

		stars_number = 0;
		star_lighting_iteration_number = 0;
		first_wire_star_position = 4;
		second_wire_star_position = 2;

		attenuation_r = (float)(FIRE_COLOR_R - BACKGROUND_COLOR_R) / FIRE_ATTENUATION_ITERATIONS_NUMBER;
		attenuation_g = (float)(FIRE_COLOR_G - BACKGROUND_COLOR_G) / FIRE_ATTENUATION_ITERATIONS_NUMBER;
		attenuation_b = (float)(FIRE_COLOR_B - BACKGROUND_COLOR_B) / FIRE_ATTENUATION_ITERATIONS_NUMBER;
	}

	void run(uint32_t *harmonic_amplitudes)
	{
		Serial.println("run");
		if (state_finished == true)
		{
			current_state++;
			if (current_state >= 5)
			{
				current_state = 0;
			}
			state_finished = false;
		}
		if (current_state == 0)
		{
			// calc node
			// Serial.println("current_state = 0");
			uint8_t max_nodes_number = NUMPIXELS / FIRE_WAVE_LENGTH - 1;
			current_node = random(1, max_nodes_number + 1);
			state_finished = true;
		}
		else if (current_state == 1)
		{
			// move fire
			Serial.println("current_state = 1");
			fire_iteration++;
			uint8_t first_fire_head_led = current_node * FIRE_WAVE_LENGTH - fire_iteration - 1;
			uint8_t second_fire_head_led = current_node * FIRE_WAVE_LENGTH + fire_iteration;

			for (int i = 0; i < NUMPIXELS; i++)
			{
				if (i == first_fire_head_led || i == second_fire_head_led)
				{
					color[i] = adjust_brightness(pixels->Color(FIRE_COLOR_R, FIRE_COLOR_G, FIRE_COLOR_B), 100);
				}
				else if (i > first_fire_head_led && i < FIRE_WAVE_LENGTH * current_node)
				{
					uint8_t temp_brightness = 100 / sqrt(i - first_fire_head_led);
					color[i] = adjust_brightness(pixels->Color(FIRE_COLOR_R, FIRE_COLOR_G, FIRE_COLOR_B), temp_brightness);
				}
				else if (i >= FIRE_WAVE_LENGTH * current_node && i < second_fire_head_led)
				{
					uint8_t temp_brightness = 100 / sqrt(second_fire_head_led - i);
					color[i] = adjust_brightness(pixels->Color(FIRE_COLOR_R, FIRE_COLOR_G, FIRE_COLOR_B), temp_brightness);
				}
				else
				{
					color[i] = pixels->Color(BACKGROUND_COLOR_R, BACKGROUND_COLOR_G, BACKGROUND_COLOR_B);
				}

				pixels->setPixelColor(i, color[i]);
			}

			if (fire_iteration >= FIRE_WAVE_LENGTH - 1)
			{
				fire_iteration = 0;
				state_finished = true;
			}
		}
		if (current_state == 2)
		{
			// attenuation
			// Serial.println("current_state = 2");
			fire_attenuation_counter++;

			for (int i = 0; i < NUMPIXELS; i++)
			{
				uint32_t prev_color = color[i];
				uint8_t prev_color_r = (uint8_t)(prev_color >> 16);
				uint8_t prev_color_g = (uint8_t)(prev_color >> 8);
				uint8_t prev_color_b = (uint8_t)prev_color;

				int current_color_r;
				int current_color_g;
				int current_color_b;

				int low_diapasone = BACKGROUND_COLOR_R - attenuation_g * 2;
				int high_diapasone = BACKGROUND_COLOR_R + attenuation_g * 2;

				current_color_r = prev_color_r - (uint8_t)(attenuation_r * fire_attenuation_counter);
				if (current_color_r < BACKGROUND_COLOR_R)
				{
					current_color_r = BACKGROUND_COLOR_R;
				}

				current_color_g = prev_color_g - (uint8_t)(attenuation_g * fire_attenuation_counter);
				if (current_color_g < BACKGROUND_COLOR_G)
				{
					current_color_g = BACKGROUND_COLOR_G;
				}

				current_color_b = prev_color_b - (uint8_t)(attenuation_b * fire_attenuation_counter);
				if (current_color_b < BACKGROUND_COLOR_B)
				{
					current_color_b = BACKGROUND_COLOR_B;
				}

				color[i] = pixels->Color((uint8_t)current_color_r, (uint8_t)current_color_g, (uint8_t)current_color_b);
				pixels->setPixelColor(i, color[i]);
			}
			if (fire_attenuation_counter >= FIRE_ATTENUATION_ITERATIONS_NUMBER / 2)
			{
				fire_attenuation_counter = 0;
				state_finished = true;
			}
			delay(40);
		}
		else if (current_state == 3)
		{
			// run stars
			// Serial.println("current_state = 3");
			star_lighting_iteration_number++;
			uint32_t current_color = 0;
			for (int i = 0; i < NUMPIXELS; i++)
			{
				if (i == current_node * FIRE_WAVE_LENGTH - first_wire_star_position || i == current_node * FIRE_WAVE_LENGTH + second_wire_star_position)
				{
					current_color = adjust_brightness(pixels->Color(STAR_COLOR_R, STAR_COLOR_G, STAR_COLOR_B),
													  star_lighting_iteration_number * (STAR_COLOR_BRIGHTNESS / STAR_LIGHTING_ITERATIONS_COUNT));
				}
				else
				{
					current_color = pixels->Color(BACKGROUND_COLOR_R, BACKGROUND_COLOR_G, BACKGROUND_COLOR_B);
				}
				pixels->setPixelColor(i, current_color);
			}
			delay(30);
			if (star_lighting_iteration_number >= STAR_LIGHTING_ITERATIONS_COUNT)
			{
				stars_number++;
				first_wire_star_position = random(0, FIRE_WAVE_LENGTH);
				second_wire_star_position = random(0, FIRE_WAVE_LENGTH);
			}
			if (stars_number >= STAR_COUNT)
			{
				stars_number = 0;
				state_finished = true;

				current_color = pixels->Color(BACKGROUND_COLOR_R, BACKGROUND_COLOR_G, BACKGROUND_COLOR_B);
				for (int i = 0; i < NUMPIXELS; i++)
				{
					pixels->setPixelColor(i, current_color);
				}
			}
		}
		else if (current_state == 4)
		{
			// wait
			Serial.println("current_state = 4");
			calculate_noize_value(harmonic_amplitudes);
			if (noize_coefficient >= 59)
			{
				state_finished = true;
			}
		}
	}

  private:
	WS2812B *pixels;
	uint8_t fire_iteration;
	uint8_t current_node;
	uint8_t current_state;
	bool state_finished;
	uint32_t color[NUMPIXELS];

	uint8_t fire_attenuation_counter;
	float attenuation_r;
	float attenuation_g;
	float attenuation_b;

	uint8_t stars_number;
	uint8_t star_lighting_iteration_number;
	uint8_t first_wire_star_position;
	uint8_t second_wire_star_position;

	uint8_t noize_coefficient;

	void calculate_noize_value(uint32_t *harmonic_amplitudes)
	{
		float temp = 0;
		for (int i = 0; i < LAST_FOURIER_VALID_INDEX; i++)
		{
			if (harmonic_amplitudes[i] > 255)
			{
				harmonic_amplitudes[i] = 255;
			}
			if (harmonic_amplitudes[i] < 0)
			{
				harmonic_amplitudes[i] = 0;
			}

			temp += harmonic_amplitudes[i] / LAST_FOURIER_VALID_INDEX;
		}
		temp /= 2.55;
		if (temp > 100.0)
		{
			temp = 100.0;
		}
		if (temp < 0)
		{
			temp = 0;
		}
		noize_coefficient = temp;
		Serial.print("noize_coefficient: ");
		Serial.println(noize_coefficient);
	}

	uint32_t adjust_brightness(uint32_t c, uint8_t amt)
	{
		// pull the R,G,B components out of the 32bit color value
		uint8_t r = (uint8_t)(c >> 16);
		uint8_t g = (uint8_t)(c >> 8);
		uint8_t b = (uint8_t)c;
		float multiplier = ((float)amt) / 100.0;
		// Scale
		r = r * multiplier;
		g = g * multiplier;
		b = b * multiplier;
		// Pack them into a 32bit color value again.
		return pixels->Color(r, g, b);
	}
};

class Liquid
{
  public:
	uint8_t const wave_length = 17;
	uint8_t peak_color_r = 200;
	uint8_t peak_color_g = 56;
	uint8_t peak_color_b = 0;
	uint8_t wave_peak_brightness = 50; // in percent

	uint8_t static_color_r = 25;
	uint8_t static_color_g = 100;
	uint8_t static_color_b = 100;
	uint8_t wave_static_brightness = 10; // in percent

	Liquid(WS2812B *pixels, uint8_t mode)
	{
		this->pixels = pixels;
		this->mode = mode;
		speed_factor = 5;

		noize_coefficient = 25;
		prev_noize_coefficient = 25;
		wave_peak_led = 11;

		wave_flooding_cycle = 0;
		color = new uint32_t[NUMPIXELS];

		red_color_cycle_change = new float[wave_length];
		green_color_cycle_change = new float[wave_length];
		blue_color_cycle_change = new float[wave_length];

		color_r_c = new uint8_t[wave_length];
		color_g_c = new uint8_t[wave_length];
		color_b_c = new uint8_t[wave_length];
	}

	void run(uint32_t *harmonic_amplitudes, uint8_t mode)
	{
		this->mode = mode;
		calculate_noize_value(harmonic_amplitudes);
		calculate_color(harmonic_amplitudes);
		for (uint8_t i = 0; i < NUMPIXELS; i++)
		{
			this->pixels->setPixelColor(i, color[i]);
		}
	}

  private:
	WS2812B *pixels;
	// mode = 0: changing wave speed (bigger noize -> faster wave)
	// mode = 1: changing wave brightness (bigger noize -> brighter wave)
	// mode = 2: changing static color brightness (bigger noize -> brighter static color)
	uint8_t mode;					// [0..2]
	uint8_t noize_coefficient;		// [0...100]
	uint8_t prev_noize_coefficient; // [0...100]
	uint8_t speed_factor;			// [1..20]

	uint8_t wave_peak_led;
	uint8_t wave_first_led;
	uint8_t wave_last_led;

	uint32_t *color;

	uint8_t static_color_r_a;
	uint8_t static_color_g_a;
	uint8_t static_color_b_a;

	float *red_color_cycle_change;
	float *green_color_cycle_change;
	float *blue_color_cycle_change;

	uint8_t *color_r_c;
	uint8_t *color_g_c;
	uint8_t *color_b_c;

	uint8_t wave_flooding_cycle;

	void calculate_noize_value(uint32_t *harmonic_amplitudes)
	{
		static uint8_t counter = 0;
		static float temp = 0;

		counter++;
		for (int i = 0; i < NUMPIXELS; i++)
		{
			if (harmonic_amplitudes[i] > 255)
			{
				harmonic_amplitudes[i] = 255;
			}
			if (harmonic_amplitudes[i] < 0)
			{
				harmonic_amplitudes[i] = 0;
			}

			temp += harmonic_amplitudes[i] / NUMPIXELS;
		}
		if (counter >= speed_factor + 1)
		{
			temp /= 2.55;
			temp /= speed_factor + 1;
			// temp *= 10;
			if (temp > 100.0)
			{
				temp = 100.0;
			}
			prev_noize_coefficient = noize_coefficient;
			noize_coefficient = temp;

			temp = 0;
			counter = 0;
			Serial.print("noize_coefficient: ");
			Serial.println(noize_coefficient);
		}
	}

	void color_init()
	{
		calculate_mode_parameter();
		calculate_wave_borders();
		uint32_t wave_peak_color = this->adjust_brightness(this->pixels->Color(this->peak_color_r,
																			   this->peak_color_g,
																			   this->peak_color_b),
														   this->wave_peak_brightness);
		uint32_t wave_static_color = this->adjust_brightness(this->pixels->Color(this->static_color_r,
																				 this->static_color_g,
																				 this->static_color_b),
															 this->wave_static_brightness);
		uint32_t wave_peak_color_copy = wave_peak_color;
		uint32_t wave_static_color_copy = wave_static_color;

		uint8_t wave_peak_color_r = (uint8_t)(wave_peak_color >> 16);
		uint8_t wave_peak_color_g = (uint8_t)(wave_peak_color >> 8);
		uint8_t wave_peak_color_b = (uint8_t)wave_peak_color;

		uint8_t wave_static_color_r = (uint8_t)(wave_static_color >> 16);
		uint8_t wave_static_color_g = (uint8_t)(wave_static_color >> 8);
		uint8_t wave_static_color_b = (uint8_t)wave_static_color;
		for (uint8_t i = 0; i < NUMPIXELS; i++)
		{
			if (i >= wave_first_led && i < wave_peak_led)
			{
				float koef = (float)(i - wave_first_led) / (wave_peak_led - wave_first_led);
				uint8_t color_r = (wave_peak_color_r - wave_static_color_r) * koef + wave_static_color_r;
				uint8_t color_g = (wave_peak_color_g - wave_static_color_g) * koef + wave_static_color_g;
				uint8_t color_b = (wave_peak_color_b - wave_static_color_b) * koef + wave_static_color_b;
				color[i] = this->pixels->Color(color_r, color_g, color_b);
			}
			else if (i == wave_peak_led)
			{
				color[i] = wave_peak_color_copy;
			}
			else if (i > wave_peak_led && i <= wave_last_led)
			{
				float koef = (float)(wave_last_led - i) / (wave_last_led - wave_peak_led);
				uint8_t color_r = (wave_peak_color_r - wave_static_color_r) * koef + wave_static_color_r;
				uint8_t color_g = (wave_peak_color_g - wave_static_color_g) * koef + wave_static_color_g;
				uint8_t color_b = (wave_peak_color_b - wave_static_color_b) * koef + wave_static_color_b;
				color[i] = this->pixels->Color(color_r, color_g, color_b);
			}
			else
			{
				color[i] = wave_static_color_copy;
			}
		}
	}

	void calculate_mode_parameter()
	{
		int noize_change = noize_coefficient - prev_noize_coefficient;
		if (mode == 0)
		{
			uint8_t temp = noize_coefficient / 2;
			if (temp > 20)
			{
				temp = 20;
			}
			temp = 21 - temp;
			int diff = speed_factor - temp;
			if (diff < 1)
			{
				speed_factor++;
			}
			else
			{
				speed_factor = temp;
			}

			if (speed_factor > 20)
			{
				speed_factor = 20;
			}
			if (speed_factor < 1)
			{
				speed_factor = 1;
			}
			Serial.print("speed_factor: ");
			Serial.println(speed_factor);
		}
		else if (mode == 1)
		{
			// changing wave brightness (bigger noize -> brighter wave)
			int new_wave_peak_brightness = prev_noize_coefficient + noize_change / 2;
			if (new_wave_peak_brightness < wave_peak_brightness)
			{
				new_wave_peak_brightness = wave_peak_brightness - 2;
			}
			if (new_wave_peak_brightness > 100)
			{
				new_wave_peak_brightness = 100;
			}
			if (new_wave_peak_brightness < 0)
			{
				new_wave_peak_brightness = 0;
			}

			wave_peak_brightness = new_wave_peak_brightness;
			Serial.print("wave_peak_brightness: ");
			Serial.println(wave_peak_brightness);
			// wave_peak_brightness = noize_coefficient;
		}
		else if (mode == 2)
		{
			//changing static color brightness (bigger noize -> brighter static color)
			int new_wave_static_brightness = prev_noize_coefficient + noize_change / 2;
			if (noize_change < -1)
			{
				new_wave_static_brightness = wave_static_brightness - 1;
			}
			if (new_wave_static_brightness > 100)
			{
				new_wave_static_brightness = 100;
			}
			if (new_wave_static_brightness < 0)
			{
				new_wave_static_brightness = 0;
			}

			wave_static_brightness = new_wave_static_brightness;
			// Serial.print("wave_static_brightness: ");
			// Serial.println(wave_static_brightness);
		}
	}

	void calculate_color(uint32_t *harmonic_amplitudes)
	{
		if (wave_flooding_cycle == 0)
		{
			calculate_colors_cycle_change(speed_factor);
		}

		if (mode == 2)
		{
			calculate_noize_value(harmonic_amplitudes);
			calculate_mode_parameter();
		}

		for (uint8_t i = 0; i < NUMPIXELS; i++)
		{
			if (i > wave_first_led && i <= wave_last_led)
			{
				uint32_t current_led_color = color[i];
				uint8_t current_led_color_r = (uint8_t)(current_led_color >> 16);
				uint8_t current_led_color_g = (uint8_t)(current_led_color >> 8);
				uint8_t current_led_color_b = (uint8_t)current_led_color;

				current_led_color_r = color_r_c[i - wave_first_led] - (red_color_cycle_change[i - wave_first_led] * wave_flooding_cycle);
				current_led_color_g = color_g_c[i - wave_first_led] - (green_color_cycle_change[i - wave_first_led] * wave_flooding_cycle);
				current_led_color_b = color_b_c[i - wave_first_led] - (blue_color_cycle_change[i - wave_first_led] * wave_flooding_cycle);

				color[i] = this->pixels->Color(current_led_color_r, current_led_color_g, current_led_color_b);
			}
			else
			{
				color[i] = adjust_brightness(pixels->Color(static_color_r, static_color_g, static_color_b), wave_static_brightness);
			}
		}

		wave_flooding_cycle++;
		if (wave_flooding_cycle > speed_factor)
		{
			wave_flooding_cycle = 0;
			if (wave_peak_led >= NUMPIXELS - wave_length / 2)
			{
				wave_peak_led = (wave_length / 2) + 1;
			}
			wave_peak_led++;

			color_init();
		}
	}

	void calculate_colors_cycle_change(uint8_t wave_flooding_cycles)
	{
		uint32_t current_color;

		uint32_t next_color;
		uint8_t color_r_n;
		uint8_t color_g_n;
		uint8_t color_b_n;

		int red_color_diff;
		int green_color_diff;
		int blue_color_diff;

		for (uint8_t i = 0; i < wave_length; i++)
		{
			uint8_t current_index = i + wave_first_led;
			uint8_t next_index = i + wave_first_led + 1;
			if (current_index >= NUMPIXELS)
			{
				current_index = 0;
			}
			if (next_index >= NUMPIXELS)
			{
				current_index = 0;
			}

			current_color = color[current_index];
			color_r_c[i] = (uint8_t)(current_color >> 16);
			color_g_c[i] = (uint8_t)(current_color >> 8);
			color_b_c[i] = (uint8_t)current_color;

			next_color = color[next_index];
			color_r_n = (uint8_t)(next_color >> 16);
			color_g_n = (uint8_t)(next_color >> 8);
			color_b_n = (uint8_t)next_color;

			red_color_diff = color_r_n - color_r_c[i];
			green_color_diff = color_g_n - color_g_c[i];
			blue_color_diff = color_b_n - color_b_c[i];

			red_color_cycle_change[i] = (float)red_color_diff / wave_flooding_cycles;
			green_color_cycle_change[i] = (float)green_color_diff / wave_flooding_cycles;
			blue_color_cycle_change[i] = (float)blue_color_diff / wave_flooding_cycles;
		}
		red_color_cycle_change[(wave_last_led - wave_first_led) / 2] *= -1;
		green_color_cycle_change[(wave_last_led - wave_first_led) / 2] *= -1;
		blue_color_cycle_change[(wave_last_led - wave_first_led) / 2] *= -1;

		red_color_cycle_change[wave_length - 1] = red_color_cycle_change[1] * (-1);
		green_color_cycle_change[wave_length - 1] = green_color_cycle_change[1] * (-1);
		blue_color_cycle_change[wave_length - 1] = blue_color_cycle_change[1] * (-1);
	}

	void calculate_wave_borders()
	{
		this->wave_first_led = wave_peak_led - (wave_length / 2);
		if (this->wave_first_led >= NUMPIXELS)
		{
			this->wave_first_led = 0;
		}
		this->wave_last_led = wave_peak_led + (wave_length / 2);
		if (this->wave_last_led >= NUMPIXELS)
		{
			this->wave_last_led = 0;
		}
	}

	uint32_t adjust_brightness(uint32_t c, uint8_t amt)
	{
		// pull the R,G,B components out of the 32bit color value
		uint8_t r = (uint8_t)(c >> 16);
		uint8_t g = (uint8_t)(c >> 8);
		uint8_t b = (uint8_t)c;
		float multiplier = ((float)amt) / 100.0;
		// Scale
		r = r * multiplier;
		g = g * multiplier;
		b = b * multiplier;
		// Pack them into a 32bit color value again.
		return pixels->Color(r, g, b);
	}
};

class CandleJars
{
  public:
	uint8_t const SPEED_FACTOR = 5; // [2..] bigger SPEED_FACTOR -> slower fire changing
	uint8_t const CANDLE_PIXELS = 3;
	uint8_t const CANDLE_SPACE = 4;
	uint8_t const MIN_CANDLE_BRIGHTNESS = 0;
	uint8_t const MAX_CANDLE_BRIGHTNESS = 100;

	uint8_t const CANDLE_COLOR_R = 255;
	uint8_t const CANDLE_COLOR_G = 40;
	uint8_t const CANDLE_COLOR_B = 0;

	CandleJars(WS2812B *pixels)
	{
		this->pixels = pixels;
		candles_number = NUMPIXELS / (CANDLE_PIXELS + CANDLE_SPACE);
		segmentSize = CANDLE_PIXELS + CANDLE_SPACE;
		fire_lifecycle_iteration = 0;
		prev_brightness = new uint8_t[candles_number];
		chage_brightness_for_step = new float[candles_number];
	}

	void run()
	{
		uint8_t current_brightness;
		for (int i = 0; i < candles_number; i++)
		{
			if (fire_lifecycle_iteration == 0)
			{
				uint8_t target_brightness = random(MIN_CANDLE_BRIGHTNESS, MAX_CANDLE_BRIGHTNESS);
				chage_brightness_for_step[i] = (target_brightness - prev_brightness[i]) / SPEED_FACTOR;
			}

			current_brightness = prev_brightness[i] + (chage_brightness_for_step[i] * fire_lifecycle_iteration);
			uint32_t new_color = adjust_brightness(pixels->Color(CANDLE_COLOR_R, CANDLE_COLOR_G, CANDLE_COLOR_B), current_brightness);
			for (int j = 0; j < CANDLE_PIXELS; j++)
			{
				pixels->setPixelColor((i * segmentSize) + j, new_color);
			}
			if (fire_lifecycle_iteration >= SPEED_FACTOR - 1)
			{
				prev_brightness[i] = current_brightness;
			}
		}
		fire_lifecycle_iteration++;
		if (fire_lifecycle_iteration >= SPEED_FACTOR)
		{
			fire_lifecycle_iteration = 0;
		}
	}

  private:
	WS2812B *pixels;
	uint8_t candles_number;
	uint8_t segmentSize;
	uint8_t fire_lifecycle_iteration;
	uint8_t *prev_brightness;
	float *chage_brightness_for_step;

	uint32_t adjust_brightness(uint32_t c, uint8_t amt)
	{
		// pull the R,G,B components out of the 32bit color value
		uint8_t r = (uint8_t)(c >> 16);
		uint8_t g = (uint8_t)(c >> 8);
		uint8_t b = (uint8_t)c;
		float multiplier = ((float)amt) / 100.0;
		// Scale
		r = r * multiplier;
		g = g * multiplier;
		b = b * multiplier;
		// Pack them into a 32bit color value again.
		return pixels->Color(r, g, b);
	}
};

CandleJars *candle_jars_mode = new CandleJars(&pixels);
Fireworks *fireworks_mode = new Fireworks(&pixels);
Liquid *liquid_mode = new Liquid(&pixels, 0);

uint16_t asqrt(uint32_t x)
{ //good enough precision, 10x faster than regular sqrt
	int32_t op, res, one;
	op = x;
	res = 0;
	/* "one" starts at the highest power of four <= than the argument. */
	one = 1 << 30; /* second-to-top bit set */
	while (one > op)
		one >>= 2;
	while (one != 0)
	{
		if (op >= res + one)
		{
			op = op - (res + one);
			res = res + 2 * one;
		}
		res /= 2;
		one /= 4;
	}
	return (uint16_t)(res);
}

void fill(uint32_t *data, uint32_t value, int len)
{
	for (int i = 0; i < len; i++)
		data[i] = value;
}

void fill(uint16_t *data, uint32_t value, int len)
{
	for (int i = 0; i < len; i++)
		data[i] = value;
}

void real_to_complex(uint16_t *in, uint32_t *out, int len)
{
	for (int i = 0; i < len; i++)
		out[i] = in[i] * 8;
}

void generate_sawtoothwave_data(uint16_t *data, uint32_t period, uint32_t amplitude, int len)
{
	for (int i = 0; i < len; i++)
	{
		data[i] = (i - period * (int(i / period))) * (amplitude / period);
	}
}

void setADCs()
{
	rcc_set_prescaler(RCC_PRESCALER_ADC, RCC_ADCPRE_PCLK_DIV_8);
	int pinMapADCin = PIN_MAP[analogInPin].adc_channel;
	adc_set_sample_rate(ADC1, ADC_SMPR_239_5); //~37.65 khz sample rate
	adc_set_reg_seqlen(ADC1, 1);
	ADC1->regs->SQR3 = pinMapADCin;
	ADC1->regs->CR2 |= ADC_CR2_CONT; // | ADC_CR2_DMA; // Set continuous mode and DMA
	ADC1->regs->CR2 |= ADC_CR2_SWSTART;
}

static void DMA1_CH1_Event()
{
	dma1_ch1_Active = 0;
}

void adc_dma_enable(const adc_dev *dev)
{
	bb_peri_set_bit(&dev->regs->CR2, ADC_CR2_DMA_BIT, 1);
}

uint16 timer_set_period(HardwareTimer timer, uint32 microseconds)
{
	if (!microseconds)
	{
		timer.setPrescaleFactor(1);
		timer.setOverflow(1);
		return timer.getOverflow();
	}
	uint32 cycles = microseconds * (72000000 / 1000000); // 72 cycles per microsecond
	uint16 ps = (uint16)((cycles >> 16) + 1);
	timer.setPrescaleFactor(ps);
	timer.setOverflow((cycles / ps) - 1);
	return timer.getOverflow();
}

void inplace_magnitude(uint32_t * target, uint16_t len)
{
	float max_freq = 9037.00; //Hz
	float step_freq = 37.65;  //Hz
	len = max_freq / step_freq;
	uint8_t steps_per_pixel = len / NUMPIXELS;
	for (int i = 0; i < len; i++)
	{
		int16_t real = target[i] & 0xFFFF;
		int16_t imag = target[i] >> 16;
		uint32_t magnitude = asqrt(real * real + imag * imag);
		target[i] = magnitude;
	}
	// linear interpulation
	for (int i = 0; i < NUMPIXELS; i++)
	{
		for (int j = 0; j < steps_per_pixel; j++)
		{
			target[i] = max(target[i], target[i * steps_per_pixel + j]);
		}
	}
}

void perform_fft(uint32_t *indata, uint32_t *outdata, const int len)
{
	cr4_fft_1024_stm32(outdata, indata, len);
	outdata[0] = 0;
	inplace_magnitude(outdata, len);
}

// Scale brightness of a color.
uint32_t adjustBrightness(uint32_t c, float amt)
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
	return pixels.Color(r, g, b);
}

void pattern_1(uint32_t *data)
{
	uint8_t scaledVolume[NUMPIXELS];
	uint8_t randomPixel;
	for (int i = 0; i < NUMPIXELS; i++)
	{
		if (data[i] < p2_vol_threshold_1)
		{						 // does the volume pass the threshold?
			scaledVolume[i] = 0; // if not set  to zero
		}
		else
		{
			scaledVolume[i] = data[i]; //* (p1_vol_threshold_3 * (i / (float)NUMPIXELS) * (i / (float)NUMPIXELS) + p1_vol_threshold_2); // scale volume to compensate for heavy bass freq
		}

		if ((scaledVolume[i] > 50) && (i > NUMPIXELS * 0.1))
		{ // if volume on high frequencies is high
			randomPixel = random(NUMPIXELS);
			fadeK[randomPixel] = (p1_coeff_smooth * (256 - scaledVolume[i])) / 255; //set initial fading koeffecient. The lower the value - the brighter the color
		}
		if (fadeK[i] <= p1_coeff_smooth)
		{ // coeff_smooth = 30
			fadeK[i]++;
			brightness[i] = 1.0 - sqrt(fadeK[i] / p1_coeff_smooth);
		}
		uint32_t color = adjustBrightness(pixels.Color(255, 0, 0), brightness[i]);
		strip[i] = color;
	}
	for (int i = 0; i < NUMPIXELS; i++)
	{
		pixels.setPixelColor(i, strip[i]); // display array [strip] on the physical strip
	}
}

void pattern_2(uint32_t *harmonic_amplitudes)
{
	// here will be "power mode"
}

void pattern_3()
{
	candle_jars_mode->run();
}

void pattern_4(uint32_t *harmonic_amplitudes)
{
	fireworks_mode->run(harmonic_amplitudes);
}

void pattern_5(uint32_t *harmonic_amplitudes)
{
	liquid_mode->run(harmonic_amplitudes, 0);
}

void pattern_6(uint32_t *harmonic_amplitudes)
{
	liquid_mode->run(harmonic_amplitudes, 1);
}

void pattern_7(uint32_t *harmonic_amplitudes)
{
	liquid_mode->run(harmonic_amplitudes, 2);
}

void setup()
{
	// power-up safety delay
	delay(500);
	Serial.begin(115200);

	//initialize LEDs
	pixels.begin();

	//initialize FFT variables
	fill(y, 0, FFTLEN);
	fill(data32, 1, FFTLEN);
	fill(data16, 1, FFTLEN);
	setADCs();
	generate_sawtoothwave_data(data16, 64, 1337, FFTLEN);
	real_to_complex(data16, data32, FFTLEN);
	perform_fft(data32, y, FFTLEN);

	randomSeed(42);
	pinMode(BTN_MODE_PIN, INPUT);
	pinMode(BTN_COLOR_PIN, INPUT);
	pinMode(BTN_POWER_PIN, INPUT);
}

void takeSamples()
{
	real_to_complex(data16, data32, FFTLEN); //clear inputs
	// perform DMA
	dma_init(DMA1);
	dma_attach_interrupt(DMA1, DMA_CH1, DMA1_CH1_Event);
	adc_dma_enable(ADC1);
	dma_setup_transfer(DMA1, DMA_CH1, &ADC1->regs->DR, DMA_SIZE_16BITS, data16, DMA_SIZE_16BITS, (DMA_MINC_MODE | DMA_TRNS_CMPLT)); // Receive buffer DMA
	dma_set_num_transfers(DMA1, DMA_CH1, FFTLEN);
	dma1_ch1_Active = 1;
	dma_enable(DMA1, DMA_CH1); // Enable the channel and start the transfer.

	while (dma1_ch1_Active)
	{
		//Wait for the DMA to complete
	}
	dma_disable(DMA1, DMA_CH1); //End of transfer, disable DMA and Continuous mode.
	perform_fft(data32, y, FFTLEN);

	if (workEnable == false)
	{
		return;
	}
	// harmonicsAmplitudeAdjustment(y);
	// Serial.println(pattern);
	if (pattern == 1)
	{
		pattern_1(y);
	}
	else if (pattern == 2)
	{
		pattern_2(y);
	}
	else if (pattern == 3)
	{
		pattern_3();
	}
	else if (pattern == 4)
	{
		pattern_4(y);
	}
	else if (pattern == 5)
	{
		pattern_5(y);
	}
	else if (pattern == 6)
	{
		pattern_6(y);
	}
	else if (pattern == 7)
	{
		pattern_7(y);
	}
}

void loop()
{
	while (1)
	{
		butonsOperations();

		takeSamples();
		pixels.show();
	}
}

void butonsOperations()
{
	// Serial.print("BTN_MODE_PIN state: ");
	// Serial.println(digitalRead(BTN_MODE_PIN));
	// Serial.print("BTN_COLOR_PIN state: ");
	// Serial.println(digitalRead(BTN_COLOR_PIN));
	// Serial.print("BTN_POWER_PIN state: ");
	// Serial.println(digitalRead(BTN_POWER_PIN));
	bool btnModeState = digitalRead(BTN_MODE_PIN);
	bool btnColorState = digitalRead(BTN_COLOR_PIN);
	bool btnPowerState = digitalRead(BTN_POWER_PIN);

	if (btnModeState != btnModePrevState)
	{
		if (btnModeState == HIGH)
		{
			Serial.println("changing pattern");
			pattern++;
			if (pattern >= 8)
			{
				pattern = 1;
			}
			clearLedStrip();
		}
	}
	btnModePrevState = btnModeState;

	if (btnColorState != btnColorPrevState)
	{
		if (btnColorState == HIGH)
		{
			Serial.println("changing color");
			colorIndex++;
			if (pattern == 3)
			{
				if (colorIndex >= EMBEDDED_COLORS_NUMBER)
				{
					colorIndex = 0;
				}

				liquid_mode->peak_color_r = LIQUID_WAVE_COLORS_SCHEMA[colorIndex][0];
				liquid_mode->peak_color_g = LIQUID_WAVE_COLORS_SCHEMA[colorIndex][1];
				liquid_mode->peak_color_b = LIQUID_WAVE_COLORS_SCHEMA[colorIndex][2];
				liquid_mode->wave_peak_brightness = LIQUID_WAVE_COLORS_SCHEMA[colorIndex][3];

				liquid_mode->static_color_r = LIQUID_STATIC_COLORS_SCHEMA[colorIndex][0];
				liquid_mode->static_color_g = LIQUID_STATIC_COLORS_SCHEMA[colorIndex][1];
				liquid_mode->static_color_b = LIQUID_STATIC_COLORS_SCHEMA[colorIndex][2];
				liquid_mode->wave_static_brightness = LIQUID_STATIC_COLORS_SCHEMA[colorIndex][3];
			}
			clearLedStrip();
		}
	}
	btnColorPrevState = btnColorState;

	if (btnPowerState != btnPowerPrevState)
	{
		if (btnPowerState == HIGH)
		{
			workEnable = !workEnable;
			clearLedStrip();
		}
	}
	btnPowerPrevState = btnPowerState;
}

void clearLedStrip()
{
	for (int i = 0; i < NUMPIXELS; i++)
	{
		pixels.setPixelColor(i, 0);
	}
	pixels.show();
}

// void harmonicsAmplitudeAdjustment(uint32_t *data)
// {
// 	static uint8_t counter = 0;

// 	if (counter >= MAX_VOLUME_FIND_ITERATION)
// 	{
// 		if (maxValueForTheLastPeriod == 0)
// 		{
// 			maxValueForTheLastPeriod = 1;
// 		}

// 		volumeMultiplier = 255 / (float)maxValueForTheLastPeriod;
// 		if (volumeMultiplier >= 10)
// 		{
// 			volumeMultiplier = 10.0;
// 		}

// 		counter = 0;
// 		maxValueForTheLastPeriod = 0;
// 		// Serial.print("volumeMultiplier: ");
// 		// Serial.println(volumeMultiplier);
// 	}

// 	for (int i = 0; i < NUMPIXELS; i++)
// 	{
// 		if (data[i] > 255)
// 		{
// 			data[i] = 255;
// 		}

// 		if (data[i] > maxValueForTheLastPeriod)
// 		{
// 			maxValueForTheLastPeriod = data[i];
// 		}

// 		data[i] *= volumeMultiplier;
// 		if (data[i] > 255)
// 		{
// 			data[i] = 255;
// 		}
// 	}
// 	counter++;
// }
