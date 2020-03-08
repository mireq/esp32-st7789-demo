// SPDX-License-Identifier: MIT

#include <math.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "font_render.h"
#include "st7789.h"


#define ST7789_GPIO_RESET GPIO_NUM_19
#define ST7789_GPIO_DC GPIO_NUM_22
#define ST7789_GPIO_MOSI GPIO_NUM_23
#define ST7789_GPIO_SCLK GPIO_NUM_18
#define ST7789_SPI_HOST VSPI_HOST
#define ST7789_DMA_CHAN 2
#define ST7789_DISPLAY_WIDTH 240
#define ST7789_DISPLAY_HEIGHT 240
#define ST7789_BUFFER_SIZE 20


extern const uint8_t ttf_start[] asm("_binary_Ubuntu_R_ttf_start");
extern const uint8_t ttf_end[] asm("_binary_Ubuntu_R_ttf_end");

static font_render_t font_render;
static font_face_t font_face;


void app_main(void)
{
	st7789_driver_t display = {
		.pin_reset = ST7789_GPIO_RESET,
		.pin_dc = ST7789_GPIO_DC,
		.pin_mosi = ST7789_GPIO_MOSI,
		.pin_sclk = ST7789_GPIO_SCLK,
		.spi_host = ST7789_SPI_HOST,
		.dma_chan = ST7789_DMA_CHAN,
		.display_width = ST7789_DISPLAY_WIDTH,
		.display_height = ST7789_DISPLAY_HEIGHT,
		.buffer_size = ST7789_BUFFER_SIZE * ST7789_DISPLAY_WIDTH, // 2 buffers with 20 lines
	};

	ESP_ERROR_CHECK(st7789_init(&display));

	st7789_reset(&display);
	st7789_lcd_init(&display);

	ESP_ERROR_CHECK(font_face_init(&font_face, ttf_start, ttf_end - ttf_start - 1));


	for (size_t tick = 0; tick < 0xffffffff; ++tick) {
		double phase = sin(((double)tick) / 100.0);
		phase = phase * phase;
		ESP_ERROR_CHECK(font_render_init(&font_render, &font_face, (phase * 200) < 12 ? 12 : (phase * 200), 1));
		font_render_glyph(&font_render, 0x21 + (tick / 12 % 0x5d));
		for (size_t block = 0; block < ST7789_DISPLAY_WIDTH; block += ST7789_BUFFER_SIZE) {
			// Background
			for (size_t y = 0; y < ST7789_BUFFER_SIZE; ++y) {
				for (size_t x = 0; x < ST7789_DISPLAY_WIDTH; ++x) {
					display.current_buffer[y * ST7789_DISPLAY_WIDTH + x] = st7789_rgb_to_color(2 * (x + tick) & 0xff, 2 * (block + y - 3 * tick) & 0xff, 2 * (block + y - x + 2*tick) & 0xff);
				}
			}

			// Text
			st7789_draw_gray2_bitmap(font_render.bitmap, display.current_buffer, 255, 255, 255, (ST7789_DISPLAY_WIDTH - font_render.bitmap_width) / 2, (ST7789_DISPLAY_HEIGHT - font_render.bitmap_height) / 2 - block, font_render.bitmap_width, font_render.bitmap_height, ST7789_DISPLAY_WIDTH, ST7789_BUFFER_SIZE);
			st7789_swap_buffers(&display);
		}
		font_render_destroy(&font_render);
	}

	font_face_destroy(&font_face);

	// Clean
	vTaskDelay(portMAX_DELAY);
	vTaskDelete(NULL);
}
