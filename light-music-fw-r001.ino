
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

//Configurable_parameters------------------------------------------------------

int bins = 10; //max = 240

uint32_t vol_threshold = 55; ////remove the noise
float low_freq_threshold = 0.15;
float high_freq_threshold = 3.5;

uint8_t pattern = 1; //1 or 2

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
		Serial.print(i);
		Serial.print("\t");
		Serial.println(data[i]);
		if (data[i] > 255)
		{
			data[i] = 255;
		}
		if ((data[i] > 200) || (current[i] < 0.01))
		{
			k[i] = (p1_coeff_fading_light * (256 - data[i])) / 255;
		}

		if (k[i] <= p1_coeff_fading_light)
		{
			k[i]++;
			current[i] = 1.0 - sqrt(k[i] / p1_coeff_fading_light);
		}

		current[i] = (current[i] + p1_coeff_smooth * current[i - 1]) / (1.0 + p1_coeff_smooth);
		uint32_t color = adjustBrightness(pixels.Color(p1_color_r, p1_color_g, p1_color_b), current[i]);

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
		if (data[i] > 255)
		{
			data[i] = 255;
		}
		if (data[i + NUMPIXELS / 2] > 255)
		{
			data[i + NUMPIXELS / 2] = 255;
		}

		if ((i < NUMPIXELS / 5) && ((data[i] > 254) || (current[i] < 0.01)))
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
	};							//Wait for the DMA to complete
	dma_disable(DMA1, DMA_CH1); //End of transfer, disable DMA and Continuous mode.
	perform_fft(data32, y, FFTLEN);

	if (pattern == 1)
	{
		pattern_1(y);
	}
	else if (pattern == 2)
	{
		pattern_2(y);
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
