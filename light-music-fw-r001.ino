// r005.0

//FFT stuff------------------------------------------------------------------
#define FFTLEN 1024
#include "cr4_fft_1024_stm32.h"
#include <SPI.h>
uint16_t data16[FFTLEN];
uint32_t data32[FFTLEN];
uint32_t y[FFTLEN];
//LED stuff-----------------------------------------------------------------
#include <WS2812B.h> //#include <NeoMaple.h>

#define NUMPIXELS 120 // number leds of the LED strip
#define NUM_REPEAT 1  // number of repeated times

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

uint32_t vol_threshold = 55; ////remove the noise
float low_freq_threshold = 0.15;
float high_freq_threshold = 3.5;

// pattern = 1
// pattern = 2
// pattern = 3: Liquid
// pattern = 4: Fireworks
// pattern = 5: CandleJars
uint8_t pattern = 4;

//For pattern_1
float p1_coeff_fading_light = 30; //30*37 = 1100 ms
float p1_coeff_smooth = 0.1;

uint8_t p1_color_r = 0;
uint8_t p1_color_g = 250;
uint8_t p1_color_b = 89;

//For pattern 2
//layer 1
float p2_wave_length = 30; //larger value =  smaller length
float p2_wave_speed = 2.0;

uint8_t p2_color_r_1 = 0;
uint8_t p2_color_g_1 = 250;
uint8_t p2_color_b_1 = 50;

//layer 2
uint8_t p2_vol_threshold = 150; //0 to 255

uint8_t p2_color_r_2 = 255;
uint8_t p2_color_g_2 = 40;
uint8_t p2_color_b_2 = 0;

//Other stuff------------------------------------------------------------------
float current[NUMPIXELS];
uint8_t k[NUMPIXELS];
uint32_t strip[NUMPIXELS];

const int8_t analogInPin = 1;

class Fireworks
{
public:
    Fireworks(WS2812B* pixels)
	{
		this->fire_iteration = 0;
		this->pixels = pixels;
		this->wave_length = 10;
		this->current_node = 1;
		this->current_state = 0;
		this->state_finished = false;
	}

    void run()
	{
		Serial.println("run");
		if (state_finished == true)
		{
			current_state++;
			if (current_state >= 3)
			{
				current_state = 0;
			}
			state_finished = false;
		}
		if (current_state == 0)
		{
			// calc node
			Serial.println("current_state = 0");
			state_finished = true;
		}
		else if (current_state == 1)
		{
			Serial.println("current_state = 1");
			// move fire
			fire_iteration++;
			uint8_t first_fire_head_led = current_node * 30 - fire_iteration - 1;
			uint8_t second_fire_head_led = current_node * 30 + fire_iteration - 1;

			uint32_t current_color = 0;
			for (int i = 0; i < 120; i++)
			{
				if (i == first_fire_head_led || i == second_fire_head_led)
				{
					current_color = adjust_brightness(pixels->Color(fire_color_r, fire_color_g, fire_color_b), 90);
				}
				else if (i > first_fire_head_led && i < 30*current_node)
				{
					uint8_t temp_brightness = 100 / ((i - first_fire_head_led)*2);
					current_color = adjust_brightness(pixels->Color(fire_color_r, fire_color_g, fire_color_b), temp_brightness);
				}
				else if (i >= 30*current_node && i < second_fire_head_led)
				{
					uint8_t temp_brightness = 100 / ((second_fire_head_led - i)*2);
					current_color = adjust_brightness(pixels->Color(fire_color_r, fire_color_g, fire_color_b), temp_brightness);
				}
				else
				{
					current_color = adjust_brightness(pixels->Color(background_color_r,
																	background_color_g,
																	background_color_b), background_color_brightness);
				}
				this->pixels->setPixelColor(i, current_color);
			}

			if (fire_iteration == 30)
			{
				fire_iteration = 0;
				state_finished = true;
			}
		}
		else if (current_state == 2)
		{
			Serial.println("current_state = 2");
			// run stars
			stars_count++;
			
			uint32_t current_color = 0;
			for (int i = 0; i < 120; i++)
			{
				if (i == 30 - stars_count || i == 30 + stars_count)
				{
					current_color = adjust_brightness(pixels->Color(star_color_r, star_color_g, star_color_b), star_color_brightness);
				}
				else
				{
					current_color = adjust_brightness(pixels->Color(background_color_r,
																	background_color_g,
																	background_color_b), background_color_brightness);
				}
				pixels->setPixelColor(i, current_color);
			}
			delay(100);
			if (stars_count >= stars_number)
			{
				stars_count = 0;
				state_finished = true;
			}
		}
	}
private:
    WS2812B* pixels;
    uint8_t fire_iteration;
    uint8_t current_node;
	uint8_t wave_length;
	uint8_t root_node;
	uint8_t current_state;
	bool state_finished;
	
	uint8_t background_color_r = 0;
	uint8_t background_color_g = 0;
	uint8_t background_color_b = 0;
	uint8_t background_color_brightness = 0;

	uint8_t fire_color_r = 200;
	uint8_t fire_color_g = 0;
	uint8_t fire_color_b = 0;

	uint8_t star_color_r = 200;
	uint8_t star_color_g = 200;
	uint8_t star_color_b = 200;
	uint8_t star_color_brightness = 90;
	uint8_t stars_count = 0;
	uint8_t stars_number = 5;

    uint32_t adjust_brightness(uint32_t c, uint8_t amt)
	{
		// pull the R,G,B components out of the 32bit color value
		uint8_t r = (uint8_t)(c >> 16);
		uint8_t g = (uint8_t)(c >> 8);
		uint8_t b = (uint8_t)c;
		float multiplier = ((float)amt)/100.0;
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
	Liquid(WS2812B* pixels)
	{
		this->pixels = pixels;
		this->wave_length = 17;
		this->wave_peak_led = 11;

		this->peak_color_r = 200;
		this->peak_color_g = 56;
		this->peak_color_b = 0;
		this->wave_peak_brightness = 50;  // in percent

		this->static_color_r = 25;
		this->static_color_g = 100;
		this->static_color_b = 200;
		this->wave_static_brightness = 10;  // in percent

		this->wave_flooding_cycle = 0;

		first_color_init();
	}

	void run()
	{
		calculate_color();
		for (uint8_t i = 0; i < PIXELS_NUMBER; i++)
		{
			this->pixels->setPixelColor(i, color[i]);


			if (i < 60)
			{
				uint32_t current_led_color = color[i];
				uint8_t current_led_color_r = (uint8_t)(current_led_color >> 16);
				uint8_t current_led_color_g = (uint8_t)(current_led_color >> 8);
				uint8_t current_led_color_b = (uint8_t)current_led_color;
				Serial.print(current_led_color_r);
				Serial.print(", ");
			}
			if (i == 60)
			{
				Serial.println(" ");
			}
		}
		
	}

private:
	uint8_t PIXELS_NUMBER = 120;
	WS2812B* pixels;
	uint8_t wave_peak_led;
	uint8_t wave_length;
	uint8_t wave_first_led;
	uint8_t wave_last_led;

	uint32_t color[120];

	uint8_t peak_color_r;
	uint8_t peak_color_g;
	uint8_t peak_color_b;
	uint8_t wave_peak_brightness;

	uint8_t static_color_r;
	uint8_t static_color_g;
	uint8_t static_color_b;
	uint8_t wave_static_brightness;

	uint8_t static_color_r_a;
	uint8_t static_color_g_a;
	uint8_t static_color_b_a;

	float red_color_cycle_change[17];
	float green_color_cycle_change[17];
	float blue_color_cycle_change[17];

	uint8_t color_r_c[17];
	uint8_t color_g_c[17];
	uint8_t color_b_c[17];

	uint8_t wave_flooding_cycle;

	void first_color_init()
	{
		calculate_wave_borders();

		uint32_t wave_peak_color = this->adjust_brightness(this->pixels->Color(this->peak_color_r,
														   this->peak_color_g,
														   this->peak_color_b), this->wave_peak_brightness);
		uint32_t wave_static_color = this->adjust_brightness(this->pixels->Color(this->static_color_r,
															 this->static_color_g,
															 this->static_color_b), this->wave_static_brightness);
		uint32_t wave_peak_color_copy = wave_peak_color;
		uint32_t wave_static_color_copy = wave_static_color;
		
		uint8_t wave_peak_color_r = (uint8_t)(wave_peak_color >> 16);
		uint8_t wave_peak_color_g = (uint8_t)(wave_peak_color >> 8);
		uint8_t wave_peak_color_b = (uint8_t)wave_peak_color;

		uint8_t wave_static_color_r = (uint8_t)(wave_static_color >> 16);
		uint8_t wave_static_color_g = (uint8_t)(wave_static_color >> 8);
		uint8_t wave_static_color_b = (uint8_t)wave_static_color;
		for (uint8_t i = 0; i < PIXELS_NUMBER; i++)
		{
			if (i >= wave_first_led && i < wave_peak_led)
			{
				float koef = (float)(i - wave_first_led)/(wave_peak_led - wave_first_led);
				uint8_t color_r = (wave_peak_color_r - wave_static_color_r)*koef + wave_static_color_r;
				uint8_t color_g = (wave_peak_color_g - wave_static_color_g)*koef + wave_static_color_g;
				uint8_t color_b = (wave_peak_color_b - wave_static_color_b)*koef + wave_static_color_b;
				color[i] = this->pixels->Color(color_r, color_g, color_b);
			}
			else if (i == wave_peak_led)
			{
				color[i] = wave_peak_color_copy;
			}
			else if (i > wave_peak_led && i <= wave_last_led)
			{
				float koef = (float)(wave_last_led - i)/(wave_last_led - wave_peak_led);
				uint8_t color_r = (wave_peak_color_r - wave_static_color_r)*koef + wave_static_color_r;
				uint8_t color_g = (wave_peak_color_g - wave_static_color_g)*koef + wave_static_color_g;
				uint8_t color_b = (wave_peak_color_b - wave_static_color_b)*koef + wave_static_color_b;
				color[i] = this->pixels->Color(color_r, color_g, color_b);
			}
			else
			{
				color[i] = wave_static_color_copy;
			}
		}
	}

	void calculate_color()
	{
		uint8_t speed_factor = 1;
		if (wave_flooding_cycle == 0)
		{
			calculate_colors_cycle_change(speed_factor);
		}
		
		// Serial.print("wave_flooding_cycle: ");
		// Serial.println(wave_flooding_cycle);
		for (uint8_t i = 0; i < PIXELS_NUMBER; i++)
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
				// Serial.println(current_led_color_r);
			}
		}

		wave_flooding_cycle++;
		if (wave_flooding_cycle > speed_factor)
		{
			wave_flooding_cycle = 0;
			if (wave_peak_led >= PIXELS_NUMBER)
			{
				wave_peak_led = 0;
			}
			wave_peak_led++;

			first_color_init();
			Serial.println(wave_first_led);
			Serial.println(wave_last_led);
		}
	}

	void calculate_colors_cycle_change(uint8_t wave_flooding_cycles)
	{
		uint32_t current_color;
		// uint8_t color_r_c;
		// uint8_t color_g_c;
		// uint8_t color_b_c;

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
			if (current_index >= PIXELS_NUMBER)
			{
				current_index = 0;
			}
			if (next_index >= PIXELS_NUMBER)
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

			// Serial.print("red_color_diff[i]: ");
			// Serial.println(red_color_diff);
			// Serial.print("green_color_diff[i]: ");
			// Serial.println(green_color_diff);
			// Serial.print("blue_color_diff[i]: ");
			// Serial.println(blue_color_diff);

			red_color_cycle_change[i] = (float)red_color_diff / wave_flooding_cycles;
			green_color_cycle_change[i] = (float)green_color_diff / wave_flooding_cycles;
			blue_color_cycle_change[i] = (float)blue_color_diff / wave_flooding_cycles;

			Serial.print("red_color_cycle_change[i]: ");
			Serial.println(red_color_cycle_change[i]);
			Serial.print("green_color_cycle_change[i]: ");
			Serial.println(green_color_cycle_change[i]);
			Serial.print("blue_color_cycle_change[i]: ");
			Serial.println(blue_color_cycle_change[i]);
			Serial.println();
		}
		red_color_cycle_change[(wave_last_led - wave_first_led)/2] *= -1;
		green_color_cycle_change[(wave_last_led - wave_first_led)/2] *= -1;
		blue_color_cycle_change[(wave_last_led - wave_first_led)/2] *= -1;

		red_color_cycle_change[wave_length - 1] = red_color_cycle_change[1] * (-1);
		green_color_cycle_change[wave_length - 1] = green_color_cycle_change[1] * (-1);
		blue_color_cycle_change[wave_length - 1] = blue_color_cycle_change[1] * (-1);

		// Serial.print("color_cycle_change: ");
		// for (int i = 0; i < wave_length; i++)
		// {
		// 	Serial.print(red_color_cycle_change[i]);
		// 	Serial.print(", ");
		// }
		// Serial.println(" ");
		// Serial.print("color_r_c: ");
		// for (int i = 0; i < wave_length; i++)
		// {
		// 	Serial.print(color_r_c[i]);
		// 	Serial.print(", ");
		// }
		// Serial.println(" ");
	}

	void calculate_wave_borders()
	{
		this->wave_first_led = wave_peak_led - (wave_length/2);
		if (this->wave_first_led >= PIXELS_NUMBER)
		{
			this->wave_first_led = 0;
		}
		this->wave_last_led = wave_peak_led + (wave_length/2);
		if (this->wave_last_led >= PIXELS_NUMBER)
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
		float multiplier = ((float)amt)/100.0;
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
    CandleJars(WS2812B* pixels)
	{
		this->pixels = pixels;
		candles_number = NUMPIXELS/(CANDLE_PIXELS + CANDLE_SPACE);
		candles_color = new uint32_t [candles_number];
	}

    void run()
	{
		uint8_t curent_brightness = 0;
		uint8_t segmentSize = CANDLE_PIXELS + CANDLE_SPACE;

		for (int i = 0; i < candles_number; i++)
		{
			max_candle_brightness = random(0, 100);
			min_candle_brightness = random(0, max_candle_brightness);
			curent_brightness = random(min_candle_brightness, max_candle_brightness);

			uint32_t prev_color = candles_color[i];
			uint8_t prev_color_r = (uint8_t)(prev_color >> 16);
			uint8_t prev_color_g = (uint8_t)(prev_color >> 8);
			uint8_t prev_color_b = (uint8_t)prev_color;

			uint32_t new_color = adjust_brightness(pixels->Color(CANDLE_COLOR_R, CANDLE_COLOR_G, CANDLE_COLOR_B), curent_brightness);
			uint8_t new_color_r = (uint8_t)(new_color >> 16);
			uint8_t new_color_g = (uint8_t)(new_color >> 8);
			uint8_t new_color_b = (uint8_t)new_color;

			uint8_t current_color_r = prev_color_r/2 + new_color_r/2;
			uint8_t current_color_g = prev_color_g/2 + new_color_g/2;
			uint8_t current_color_b = prev_color_b/2 + new_color_b/2;

			candles_color[i] = pixels->Color(current_color_r, current_color_g, current_color_b);
			for (int j = 0; j < CANDLE_PIXELS; j++)
			{
				pixels->setPixelColor((i*segmentSize) + j, candles_color[i]);
			}
		}
		delay(50);
	}
private:
    WS2812B* pixels;
	uint8_t const CANDLE_PIXELS = 3;
	uint8_t const CANDLE_SPACE = 4;
	uint8_t  min_candle_brightness = 0;
	uint8_t  max_candle_brightness = 100;

	uint8_t const CANDLE_COLOR_R = 255;
	uint8_t const CANDLE_COLOR_G = 40;
	uint8_t const CANDLE_COLOR_B = 0;

	uint8_t candles_number;
	uint32_t* candles_color;

    uint32_t adjust_brightness(uint32_t c, uint8_t amt)
	{
		// pull the R,G,B components out of the 32bit color value
		uint8_t r = (uint8_t)(c >> 16);
		uint8_t g = (uint8_t)(c >> 8);
		uint8_t b = (uint8_t)c;
		float multiplier = ((float)amt)/100.0;
		// Scale
		r = r * multiplier;
		g = g * multiplier;
		b = b * multiplier;
		// Pack them into a 32bit color value again.
		return pixels->Color(r, g, b);
	}
};

Liquid* mode3 = new Liquid(&pixels);
Fireworks* mode4 = new Fireworks(&pixels);
CandleJars* mode5 = new CandleJars(&pixels);

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

void inplace_magnitude(uint32_t *target, uint16_t len)
{
	float max_freq = 9037.00; //Hz
	float step_freq = 37.65;  //Hz
	uint8_t steps_per_pixel;
	uint8_t pixels_per_bin;
	len = max_freq / step_freq; // 240 bins

	for (int i = 0; i < len; i++)
	{
		int16_t real = target[i] & 0xFFFF;
		int16_t imag = target[i] >> 16;
		uint32_t magnitude = asqrt(real * real + imag * imag);
		magnitude = magnitude * (high_freq_threshold * (i / (float)len) * (i / (float)len) + low_freq_threshold);
		if (magnitude > vol_threshold)
		{
			target[i] = magnitude;
		}
		else
		{
			target[i] = 0;
		}
	}
	uint32_t temp_data[bins];
	pixels_per_bin = NUMPIXELS / bins;
	for (int i = 0; i < bins; i++)
	{
		temp_data[i] = 0;
		for (int j = 0; j < (len / bins); j++)
		{
			temp_data[i] = max(temp_data[i], target[i * (len / bins) + j]);
		}
	}
	for (int i = 0; i < bins; i++)
	{
		for (int j = 0; j < pixels_per_bin; j++)
		{
			target[i * pixels_per_bin + j] = temp_data[i];
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
	for (int i = 0; i < NUMPIXELS; i++)
	{
		Serial.println(i);
		// Serial.print("\t");
		// Serial.println(data[i]);
		if (data[i] > 255)
		{
			data[i] = 255;
		}
		// якщо висока амплітуда або вже затухло
		if ((data[i] > 200) || (current[i] < 0.01))
		{
			// k[i] diapasone 0..30; bigger harmonic -> smaller k[i] // k[i] - attenuation iteration
			// рахуємо ітерацію затухання, всього 30 (30 значить вже мінімум, 0 - максимум)
			k[i] = (p1_coeff_fading_light * (256 - data[i])) / 255;
		}

		// always? рахуємо інтенсивнсть
		if (k[i] <= p1_coeff_fading_light)  // 30
		{
			k[i]++;  // k[i]: 1..31
			current[i] = 1.0 - sqrt(k[i] / p1_coeff_fading_light);  // current[i]: 0..1
		}
		Serial.print("k[i]: ");
		Serial.println(k[i]);

		// розмазуєм інтенсивності
		current[i] = (current[i] + p1_coeff_smooth * current[i - 1]) / (1.0 + p1_coeff_smooth);  // розмазування
		Serial.print("current[i]: ");
		Serial.println(current[i]);

		uint32_t color = adjustBrightness(pixels.Color(p1_color_r, p1_color_g, p1_color_b), current[i]);
		Serial.print("color: ");
		Serial.println(color);
		Serial.println(" ");

		for (int j = 0; j < NUM_REPEAT; j++)
		{
			pixels.setPixelColor(j * NUMPIXELS + i, color);
		}
	}
}

void pattern_2(uint32_t *data)
{
	for (int i = 0; i < NUMPIXELS / 2; i++)
	{
		Serial.println(i);
		if (data[i] > 255)
		{
			data[i] = 255;
		}
		if (data[i + NUMPIXELS / 2] > 255)
		{
			data[i + NUMPIXELS / 2] = 255;
		}

		if ((i < NUMPIXELS / 5) && ((data[i] > 254) || (current[i] < 0.01)))  // відсікаються низькі частоти
		{
			k[i] = (p2_wave_length * (256 - data[i])) / 255;
		}

		if (k[i] <= p2_wave_length)
		{ //speed
			k[i]++;
			current[i] = 1.0 - sqrt(k[i] / p2_wave_length);
		}
		current[i] = (current[i] + p2_wave_speed * current[i - 1]) / (1 + p2_wave_speed);
		uint32_t color;
		
		if ((data[i + NUMPIXELS / 2] > p2_vol_threshold))
		{
			color = adjustBrightness(pixels.Color(p2_color_r_2, p2_color_g_2, p2_color_b_2), (float)data[i] / 255.0);
		}
		else
		{
			color = adjustBrightness(pixels.Color(p2_color_r_1, p2_color_g_1, p2_color_b_1), current[i]);
		}
		for (int j = 0; j < NUM_REPEAT; j++)
		{
			pixels.setPixelColor(j * NUMPIXELS + NUMPIXELS / 2 + i, color);
			pixels.setPixelColor(j * NUMPIXELS + NUMPIXELS / 2 - i, color);
		}
	}
}

void pattern_3()
{
	mode3->run();
}

void pattern_4()
{
	mode4->run();
}

void pattern_5()
{
	mode5->run();
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
		pattern_4();
	}
	else if (pattern == 5)
	{
		pattern_5();
	}
}

void loop()
{
	while (1)
	{
		takeSamples();
		pixels.show();
	}
}
