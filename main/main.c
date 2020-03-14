// SPDX-License-Identifier: MIT

#include <math.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "soc/cpu.h"

#include "unicode.h"
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
static font_render_t font_render2;
static font_face_t font_face;


#define DRAW_EVENT_START 0xfffc
#define DRAW_EVENT_END 0xfffd
#define DRAW_EVENT_FRAME_START 0xfffe
#define DRAW_EVENT_FRAME_END 0xffff
#define DRAW_EVENT_CONTROL DRAW_EVENT_START


typedef struct draw_event_param {
	uint64_t frame;
	uint64_t total_frame;
	uint64_t duration;
	void *user_data;
} draw_event_param_t;

typedef void (*draw_callback)(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param);

typedef struct draw_element {
	draw_callback callback;
	void *user_data;
} draw_element_t;

typedef struct animation_step {
	const uint64_t duration;
	const draw_element_t *draw_elements;
} animation_step_t;


static void render_text(const char *text, font_render_t *render, st7789_driver_t *driver, int src_x, int src_y, int y, uint8_t color_r, uint8_t color_g, uint8_t color_b) {
	if (src_y - y >= ST7789_BUFFER_SIZE || src_y + (int)render->max_pixel_height - y < 0) {
		return;
	}

	while (*text) {
		uint32_t glyph;
		text += u8_decode(&glyph, text);
		font_render_glyph(render, glyph);
		st7789_draw_gray2_bitmap(render->bitmap, driver->current_buffer, color_r, color_g, color_b, src_x + render->bitmap_left, render->max_pixel_height - render->origin - render->bitmap_top + src_y - y, render->bitmap_width, render->bitmap_height, driver->display_width, ST7789_BUFFER_SIZE);
		src_x += render->advance;
	}
}


#define GREEN_BACKGROUND_COLOR 80

void gradient(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	if (y >= DRAW_EVENT_CONTROL) {
		if (y == DRAW_EVENT_START) {
			ESP_ERROR_CHECK(font_render_init(&font_render, &font_face, 24, 16));
		}
		else if (y == DRAW_EVENT_END) {
			font_render_destroy(&font_render);
		}
		return;
	}

	uint16_t cursor_x = driver->display_width - 1;
	uint16_t cursor_y = y - 1;
	int color_val = 0;
	const bool strips[7][3] = {
		{1, 1, 1},
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1},
		{1, 1, 0},
		{0, 1, 1},
		{1, 0, 1},
	};
	for (size_t i = 0; i < driver->buffer_size; ++i) {
		cursor_x++;
		if (cursor_x == driver->display_width) {
			cursor_x = 0;
			cursor_y++;
		}
		color_val = cursor_x * driver->display_width / 256;
		const bool *strip = strips[cursor_y * 7 / driver->display_width];
		const uint8_t color_r = strip[0] ? color_val : 0;
		const uint8_t color_g = strip[1] ? color_val : 0;
		const uint8_t color_b = strip[2] ? color_val : 0;
		if (param->frame > (param->duration >> 1)) {
			driver->current_buffer[i] = st7789_rgb_to_color_dither(
				color_r,
				color_g,
				color_b,
				cursor_x,
				cursor_y
			);
		}
		else {
			driver->current_buffer[i] = st7789_rgb_to_color(
				color_r,
				color_g,
				color_b
			);
		}
	}
	if (param->frame > (param->duration >> 1)) {
		render_text("With dithering", &font_render, driver, 8, 210, y, 255, 255, 255);
	}
	else {
		render_text("Without dithering", &font_render, driver, 8, 210, y, 255, 255, 255);
	}
}

void fade_in_green(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	if (y >= DRAW_EVENT_CONTROL) {
		return;
	}
	const int start_color = 0;
	const int end_color = GREEN_BACKGROUND_COLOR;
	const float transition_position = ((float)param->frame + 1.0) / (float)param->duration;
	const int color_value = start_color + (end_color - start_color) * transition_position;
	uint16_t cursor_x = 0;
	uint16_t cursor_y = y;
	for (size_t i = 0; i < driver->buffer_size; ++i) {
		driver->current_buffer[i] = st7789_rgb_to_color_dither(0, color_value, 0, cursor_x, cursor_y);
		cursor_x++;
		if (cursor_x == driver->display_width) {
			cursor_x = 0;
			cursor_y++;
		}
	}
}


void green_background(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	if (y >= DRAW_EVENT_CONTROL) {
		return;
	}
	st7789_color_t color = st7789_rgb_to_color(0, GREEN_BACKGROUND_COLOR, 0);
	for (size_t i = 0; i < driver->buffer_size; ++i) {
		driver->current_buffer[i] = color;
	}
}


void fade_in_a(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	if (y >= DRAW_EVENT_CONTROL) {
		if (y == DRAW_EVENT_START) {
			ESP_ERROR_CHECK(font_render_init(&font_render, &font_face, 200, 1));
			font_render_glyph(&font_render, (uint32_t)'A');
		}
		else if (y == DRAW_EVENT_END) {
			font_render_destroy(&font_render);
		}
		return;
	}

	float transition_position = ((float)param->frame + 1.0) / (float)param->duration;
	transition_position = transition_position * transition_position;
	const int color_r = 255 * transition_position;
	const int color_g = GREEN_BACKGROUND_COLOR + 4 + (255 - GREEN_BACKGROUND_COLOR - 4) * transition_position;
	const int color_b = color_r;

	st7789_draw_gray2_bitmap(
		font_render.bitmap,
		driver->current_buffer,
		color_r, color_g, color_b,
		(driver->display_width - font_render.bitmap_width) / 2,
		(driver->display_height - font_render.max_pixel_height) / 2 - y - font_render.bitmap_top - font_render.origin + font_render.max_pixel_height,
		font_render.bitmap_width,
		font_render.bitmap_height,
		driver->display_width,
		ST7789_BUFFER_SIZE
	);
}


void draw_alphabet(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	if (y >= DRAW_EVENT_CONTROL) {
		if (y == DRAW_EVENT_START) {
			ESP_ERROR_CHECK(font_render_init(&font_render, &font_face, 200, 1));
		}
		else if (y == DRAW_EVENT_END) {
			font_render_destroy(&font_render);
		}
		else if (y == DRAW_EVENT_FRAME_START) {
			float transition_position = ((float)param->frame + 1.0) / (float)param->duration;
			transition_position = (transition_position * transition_position + transition_position) / 2;
			uint32_t glyph = 0x21 + 0x5d * transition_position;
			if (transition_position >= 0.99) {
				glyph = (uint32_t)'A';
			}
			font_render_glyph(&font_render, glyph);
		}
		return;
	}

	st7789_draw_gray2_bitmap(
		font_render.bitmap,
		driver->current_buffer,
		255, 255, 255,
		(driver->display_width - font_render.bitmap_width) / 2,
		(driver->display_height - font_render.max_pixel_height) / 2 - y - font_render.bitmap_top - font_render.origin + font_render.max_pixel_height,
		font_render.bitmap_width,
		font_render.bitmap_height,
		driver->display_width,
		ST7789_BUFFER_SIZE
	);
}


void shrink_a(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	float transition_position = ((float)param->frame + 1.0) / (float)param->duration;
	int vertical_move = transition_position * driver->display_height / 4;
	if (y >= DRAW_EVENT_CONTROL) {
		if (y == DRAW_EVENT_FRAME_START) {
			transition_position = 1.0 - transition_position;
			transition_position = 1.0 - (transition_position * transition_position);
			ESP_ERROR_CHECK(font_render_init(&font_render, &font_face, 200 - 140 * transition_position, 1));
			font_render_glyph(&font_render, (uint32_t)'A');
		}
		else if (y == DRAW_EVENT_FRAME_END) {
			font_render_destroy(&font_render);
		}
		return;
	}

	st7789_draw_gray2_bitmap(
		font_render.bitmap,
		driver->current_buffer,
		255, 255, 255,
		(driver->display_width - font_render.bitmap_width) / 2,
		(driver->display_height - font_render.max_pixel_height) / 2 - y - vertical_move - font_render.bitmap_top - font_render.origin + font_render.max_pixel_height,
		font_render.bitmap_width,
		font_render.bitmap_height,
		driver->display_width,
		ST7789_BUFFER_SIZE
	);
}


void perfect_rendering(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	float transition_position = ((float)param->frame + 1.0) / (float)param->duration;

	if (y >= DRAW_EVENT_CONTROL) {
		if (y == DRAW_EVENT_START) {
			ESP_ERROR_CHECK(font_render_init(&font_render, &font_face, 60, 1));
			ESP_ERROR_CHECK(font_render_init(&font_render2, &font_face, 16, 32));
			font_render_glyph(&font_render, (uint32_t)'A');
		}
		else if (y == DRAW_EVENT_END) {
			font_render_destroy(&font_render2);
			font_render_destroy(&font_render);
		}
		return;
	}

	{
		float transition_position_font1 = 0.0;
		if (transition_position < 0.2) {
			transition_position_font1 = transition_position * 4.0;
		}
		else if (transition_position > 0.8) {
			transition_position_font1 = transition_position;
		}
		else {
			transition_position_font1 = 0.8;
		}

		const int color_r = 255 - 255 * transition_position_font1;
		const int color_g = GREEN_BACKGROUND_COLOR - 4 + (1.0 - transition_position_font1) * (255 - GREEN_BACKGROUND_COLOR - 4);
		const int color_b = color_r;

		int vertical_move = driver->display_height / 4;

		st7789_draw_gray2_bitmap(
			font_render.bitmap,
			driver->current_buffer,
			color_r, color_g, color_b,
			(driver->display_width - font_render.bitmap_width) / 2,
			(driver->display_height - font_render.max_pixel_height) / 2 - y - vertical_move - font_render.bitmap_top - font_render.origin + font_render.max_pixel_height,
			font_render.bitmap_width,
			font_render.bitmap_height,
			driver->display_width,
			ST7789_BUFFER_SIZE
		);
	}

	{
		float transition_position_font1 = 0.0;
		float transition_position_font2 = 0.0;
		if (transition_position < 0.1) {
			transition_position_font1 = 0;
			transition_position_font2 = 0;
		}
		else if (transition_position < 0.3) {
			transition_position_font1 = (transition_position - 0.1) * 5;
			transition_position_font2 = (transition_position - 0.1) * 5;
		}
		else if (transition_position < 0.9) {
			transition_position_font1 = 1.0;
			transition_position_font2 = 1.0;
		}
		else {
			transition_position_font1 = 1.0 - (transition_position - 0.9) * 10;
			transition_position_font2 = 1.0;
		}

		transition_position_font1 = transition_position_font1 * transition_position_font1;

		const int color_r = 255 * transition_position_font1;
		const int color_g = GREEN_BACKGROUND_COLOR + 4 + (255 - GREEN_BACKGROUND_COLOR - 4) * transition_position_font1;
		const int color_b = color_r;

		render_text("Perfectly readable", &font_render2, driver, 30 - (1.0 - transition_position_font2) * 100, 110, y, color_r, color_g, color_b);
	}

	{
		float transition_position_font1 = 0.0;
		if (transition_position < 0.4) {
			transition_position_font1 = 0;
		}
		else if (transition_position < 0.6) {
			transition_position_font1 = (transition_position - 0.4) * 5;
		}
		else if (transition_position < 0.9) {
			transition_position_font1 = 1.0;
		}
		else {
			transition_position_font1 = 1.0 - (transition_position - 0.9) * 10;
		}

		transition_position_font1 = transition_position_font1 * transition_position_font1;

		const int color_r = 255 * transition_position_font1;
		const int color_g = GREEN_BACKGROUND_COLOR + 4 + (255 - GREEN_BACKGROUND_COLOR - 4) * transition_position_font1;
		const int color_b = color_r;

		render_text("even small fonts", &font_render2, driver, 90, 128, y, color_r, color_g, color_b);
	}
}


void fade_out_green(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	if (y >= DRAW_EVENT_CONTROL) {
		return;
	}
	const int start_color = GREEN_BACKGROUND_COLOR;
	const int end_color = 0;
	const float transition_position = ((float)param->frame + 1.0) / (float)param->duration;
	const int color_value = start_color + (end_color - start_color) * transition_position;
	uint16_t cursor_x = 0;
	uint16_t cursor_y = y;
	for (size_t i = 0; i < driver->buffer_size; ++i) {
		driver->current_buffer[i] = st7789_rgb_to_color_dither(0, color_value, 0, cursor_x, cursor_y);
		cursor_x++;
		if (cursor_x == driver->display_width) {
			cursor_x = 0;
			cursor_y++;
		}
	}
}


static const uint8_t sin_table[] = {128, 129, 130, 130, 131, 132, 133, 133, 134, 135, 136, 137, 137, 138, 139, 140, 140, 141, 142, 143, 144, 144, 145, 146, 147, 147, 148, 149, 150, 151, 151, 152, 153, 154, 154, 155, 156, 157, 157, 158, 159, 160, 160, 161, 162, 163, 164, 164, 165, 166, 167, 167, 168, 169, 169, 170, 171, 172, 172, 173, 174, 175, 175, 176, 177, 178, 178, 179, 180, 180, 181, 182, 183, 183, 184, 185, 185, 186, 187, 187, 188, 189, 189, 190, 191, 192, 192, 193, 194, 194, 195, 196, 196, 197, 198, 198, 199, 199, 200, 201, 201, 202, 203, 203, 204, 205, 205, 206, 206, 207, 208, 208, 209, 209, 210, 211, 211, 212, 212, 213, 214, 214, 215, 215, 216, 216, 217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 224, 224, 225, 225, 226, 226, 227, 227, 228, 228, 229, 229, 229, 230, 230, 231, 231, 232, 232, 233, 233, 234, 234, 234, 235, 235, 236, 236, 237, 237, 237, 238, 238, 239, 239, 239, 240, 240, 240, 241, 241, 242, 242, 242, 243, 243, 243, 244, 244, 244, 245, 245, 245, 245, 246, 246, 246, 247, 247, 247, 248, 248, 248, 248, 249, 249, 249, 249, 250, 250, 250, 250, 250, 251, 251, 251, 251, 251, 252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 254, 254, 254, 254, 254, 254, 254, 254, 253, 253, 253, 253, 253, 253, 253, 252, 252, 252, 252, 252, 251, 251, 251, 251, 251, 250, 250, 250, 250, 250, 249, 249, 249, 249, 248, 248, 248, 248, 247, 247, 247, 246, 246, 246, 245, 245, 245, 245, 244, 244, 244, 243, 243, 243, 242, 242, 242, 241, 241, 240, 240, 240, 239, 239, 239, 238, 238, 237, 237, 237, 236, 236, 235, 235, 234, 234, 234, 233, 233, 232, 232, 231, 231, 230, 230, 229, 229, 229, 228, 228, 227, 227, 226, 226, 225, 225, 224, 224, 223, 222, 222, 221, 221, 220, 220, 219, 219, 218, 218, 217, 216, 216, 215, 215, 214, 214, 213, 212, 212, 211, 211, 210, 209, 209, 208, 208, 207, 206, 206, 205, 205, 204, 203, 203, 202, 201, 201, 200, 199, 199, 198, 198, 197, 196, 196, 195, 194, 194, 193, 192, 192, 191, 190, 189, 189, 188, 187, 187, 186, 185, 185, 184, 183, 183, 182, 181, 180, 180, 179, 178, 178, 177, 176, 175, 175, 174, 173, 172, 172, 171, 170, 169, 169, 168, 167, 167, 166, 165, 164, 164, 163, 162, 161, 160, 160, 159, 158, 157, 157, 156, 155, 154, 154, 153, 152, 151, 151, 150, 149, 148, 147, 147, 146, 145, 144, 144, 143, 142, 141, 140, 140, 139, 138, 137, 137, 136, 135, 134, 133, 133, 132, 131, 130, 130, 129, 128, 127, 126, 126, 125, 124, 123, 123, 122, 121, 120, 119, 119, 118, 117, 116, 116, 115, 114, 113, 112, 112, 111, 110, 109, 109, 108, 107, 106, 105, 105, 104, 103, 102, 102, 101, 100, 99, 99, 98, 97, 96, 96, 95, 94, 93, 92, 92, 91, 90, 89, 89, 88, 87, 87, 86, 85, 84, 84, 83, 82, 81, 81, 80, 79, 78, 78, 77, 76, 76, 75, 74, 73, 73, 72, 71, 71, 70, 69, 69, 68, 67, 67, 66, 65, 64, 64, 63, 62, 62, 61, 60, 60, 59, 58, 58, 57, 57, 56, 55, 55, 54, 53, 53, 52, 51, 51, 50, 50, 49, 48, 48, 47, 47, 46, 45, 45, 44, 44, 43, 42, 42, 41, 41, 40, 40, 39, 38, 38, 37, 37, 36, 36, 35, 35, 34, 34, 33, 32, 32, 31, 31, 30, 30, 29, 29, 28, 28, 27, 27, 27, 26, 26, 25, 25, 24, 24, 23, 23, 22, 22, 22, 21, 21, 20, 20, 19, 19, 19, 18, 18, 17, 17, 17, 16, 16, 16, 15, 15, 14, 14, 14, 13, 13, 13, 12, 12, 12, 11, 11, 11, 11, 10, 10, 10, 9, 9, 9, 8, 8, 8, 8, 7, 7, 7, 7, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 15, 15, 16, 16, 16, 17, 17, 17, 18, 18, 19, 19, 19, 20, 20, 21, 21, 22, 22, 22, 23, 23, 24, 24, 25, 25, 26, 26, 27, 27, 27, 28, 28, 29, 29, 30, 30, 31, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37, 38, 38, 39, 40, 40, 41, 41, 42, 42, 43, 44, 44, 45, 45, 46, 47, 47, 48, 48, 49, 50, 50, 51, 51, 52, 53, 53, 54, 55, 55, 56, 57, 57, 58, 58, 59, 60, 60, 61, 62, 62, 63, 64, 64, 65, 66, 67, 67, 68, 69, 69, 70, 71, 71, 72, 73, 73, 74, 75, 76, 76, 77, 78, 78, 79, 80, 81, 81, 82, 83, 84, 84, 85, 86, 87, 87, 88, 89, 89, 90, 91, 92, 92, 93, 94, 95, 96, 96, 97, 98, 99, 99, 100, 101, 102, 102, 103, 104, 105, 105, 106, 107, 108, 109, 109, 110, 111, 112, 112, 113, 114, 115, 116, 116, 117, 118, 119, 119, 120, 121, 122, 123, 123, 124, 125, 126, 126, 127};


static inline uint8_t __attribute__((always_inline)) fast_sin(int value) {
	return sin_table[value & 0x3ff];
}

/*
static const uint8_t sin_table[] = {0, 2, 3, 5, 6, 8, 9, 11, 13, 14, 16, 17, 19, 20, 22, 23, 25, 27, 28, 30, 31, 33, 34, 36, 37, 39, 41, 42, 44, 45, 47, 48, 50, 51, 53, 54, 56, 57, 59, 60, 62, 63, 65, 67, 68, 70, 71, 73, 74, 76, 77, 79, 80, 81, 83, 84, 86, 87, 89, 90, 92, 93, 95, 96, 98, 99, 100, 102, 103, 105, 106, 108, 109, 110, 112, 113, 115, 116, 117, 119, 120, 122, 123, 124, 126, 127, 128, 130, 131, 132, 134, 135, 136, 138, 139, 140, 142, 143, 144, 146, 147, 148, 149, 151, 152, 153, 154, 156, 157, 158, 159, 161, 162, 163, 164, 165, 167, 168, 169, 170, 171, 172, 174, 175, 176, 177, 178, 179, 180, 181, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 208, 209, 210, 211, 212, 213, 214, 215, 215, 216, 217, 218, 219, 220, 220, 221, 222, 223, 223, 224, 225, 226, 226, 227, 228, 228, 229, 230, 231, 231, 232, 232, 233, 234, 234, 235, 236, 236, 237, 237, 238, 238, 239, 240, 240, 241, 241, 242, 242, 243, 243, 244, 244, 244, 245, 245, 246, 246, 247, 247, 247, 248, 248, 248, 249, 249, 249, 250, 250, 250, 251, 251, 251, 252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};


static inline uint8_t __attribute__((always_inline)) fast_sin(int value) {
	uint8_t table_position = value & 0xff;
	if (value & 0x0100) {
		table_position = 255 - (value & 0xff);
	}
	uint8_t sin_val = sin_table[table_position] >> 1;
	if (value & 0x0200) {
		return 128 - sin_val;
	}
	else {
		return 128 + sin_val;
	}
}
*/



void complex_text_demo(st7789_driver_t *driver, uint16_t y, draw_event_param_t *param) {
	if (y >= DRAW_EVENT_CONTROL) {
		if (y == DRAW_EVENT_START) {
			ESP_ERROR_CHECK(font_render_init(&font_render2, &font_face, 14, 48));
		}
		else if (y == DRAW_EVENT_END) {
			font_render_destroy(&font_render2);
		}
		else if (y == DRAW_EVENT_FRAME_START) {
			if (param->frame > 1200 - 240) {
				uint32_t glyph = 0x21 + ((param->frame >> 5) % 0x5d);
				ESP_ERROR_CHECK(font_render_init(&font_render, &font_face, (fast_sin(param->frame << 2) >> 1) + 14, 1));
				font_render_glyph(&font_render, glyph);
			}
		}
		else if (y == DRAW_EVENT_FRAME_END) {
			if (param->frame > 1200 - 240) {
				font_render_destroy(&font_render);
			}
		}
		return;
	}

	int cursor_x = driver->display_width - 1;
	int cursor_y = y - 1;
	const int frame = (int)param->frame;
	const int plasma_shift = frame < 256 ? 1 : 2;

	const int frame_1 = frame << 1;
	const int frame_2 = frame << 2;
	const int frame_7 = frame * 7;
	for (size_t i = 0; i < driver->buffer_size; ++i) {
		cursor_x++;
		if (cursor_x == driver->display_width) {
			cursor_x = 0;
			cursor_y++;
		}

		if (frame + cursor_y < 1200) {
			const int cursor_x_1 = cursor_x << 1;
			const int cursor_x_2 = cursor_x << 2;
			const int cursor_y_1 = cursor_y << 1;
			const int cursor_y_2 = cursor_y << 2;

			uint16_t plasma_value = fast_sin(cursor_x_2 + cursor_y_1 + frame_2);
			plasma_value += fast_sin(fast_sin(((cursor_y_1 + frame) << 1) + cursor_x) + frame_7);
			plasma_value >>= plasma_shift;

			uint16_t color_r = plasma_value;

			plasma_value = fast_sin(cursor_x + cursor_y_2 + frame_1);
			plasma_value += fast_sin(fast_sin(((cursor_x_1 + frame) << 1) + cursor_y) + frame_1);
			plasma_value >>= plasma_shift;

			uint16_t color_b = plasma_value;

			if (frame < 256) {
				if (frame < 64) {
					color_r = (color_r * frame) >> 6;
					color_b = (color_b * frame) >> 6;
				}
				if (frame > 128) {
					color_r = (color_r * (32 + ((256 - frame) >> 2))) >> 6;
					color_b = (color_b * (32 + ((256 - frame) >> 2))) >> 6;
				}
			}
			driver->current_buffer[i] = st7789_rgb_to_color_dither(color_r, (color_r >> 1) + (color_b >> 1), color_b, cursor_x, cursor_y);
		}
		else {
			int vertical_move = 1200 - frame;
			if (vertical_move < 0) {
				vertical_move = 0;
			}
			const int darken = (cursor_y - vertical_move) > 40 && (cursor_y - vertical_move) < 200 && cursor_x > 40 && cursor_x < 200 ? 1 : 0;
			driver->current_buffer[i] = st7789_rgb_to_color((2 * (cursor_x - frame) & 0xff) >> darken, (2 * (cursor_y + 3 * frame) & 0xff) >> darken, (2 * (cursor_y - cursor_x - 2*frame) & 0xff) >> darken);
		}
	}

	if (frame > 192 && frame < 1200) {
		const int y_shift = 240 + ((192 - frame) >> 1);
		const int line_height = 20;
		render_text("Lorem ipsum dolor sit amet,", &font_render2, driver, 8, y_shift, y, 255, 255, 255);
		render_text("consectetur adipiscing elit.", &font_render2, driver, 8, y_shift + line_height * 1, y, 255, 255, 255);
		render_text("Pellentesque tristique quam sit", &font_render2, driver, 8, y_shift + line_height * 2, y, 255, 255, 255);
		render_text("amet dolor sagittis lacinia.", &font_render2, driver, 8, y_shift + line_height * 3, y, 255, 255, 255);
		render_text("Phasellus non dui sed orci", &font_render2, driver, 8, y_shift + line_height * 4, y, 255, 255, 255);
		render_text("vehicula faucibus ut vitae dui.", &font_render2, driver, 8, y_shift + line_height * 5, y, 255, 255, 255);
		render_text("Duis pulvinar sem risus, quis", &font_render2, driver, 8, y_shift + line_height * 6, y, 255, 255, 255);
		render_text("bibendum elit consequat vel.", &font_render2, driver, 8, y_shift + line_height * 7, y, 255, 255, 255);
		render_text("Cras eget fermentum magna.", &font_render2, driver, 8, y_shift + line_height * 8, y, 255, 255, 255);
		render_text("Maecenas eu pretium diam,", &font_render2, driver, 8, y_shift + line_height * 9, y, 255, 255, 255);
		render_text("sed tempor ex.", &font_render2, driver, 8, y_shift + line_height * 10, y, 255, 255, 255);
	}

	if (frame > 1200 - 240) {
		int vertical_move = 1200 - frame;
		if (vertical_move < 0) {
			vertical_move = 0;
		}
		st7789_draw_gray2_bitmap(
			font_render.bitmap,
			driver->current_buffer,
			255, 255, 255,
			(driver->display_width - font_render.bitmap_width) / 2,
			(driver->display_height - font_render.max_pixel_height) / 2 - y - font_render.bitmap_top - font_render.origin + font_render.max_pixel_height + vertical_move,
			font_render.bitmap_width,
			font_render.bitmap_height,
			driver->display_width,
			ST7789_BUFFER_SIZE
		);
	}
}


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

	while (1) {
		ESP_ERROR_CHECK(font_face_init(&font_face, ttf_start, ttf_end - ttf_start - 1));

		st7789_reset(&display);
		st7789_lcd_init(&display);


		// Animation
		const draw_element_t noop_layers[] = {
			{NULL, NULL},
		};
		const draw_element_t gradient_layers[] = {
			{gradient, NULL},
			{NULL, NULL},
		};
		const draw_element_t fade_in_green_layers[] = {
			{fade_in_green, NULL},
			{NULL, NULL},
		};
		const draw_element_t fade_in_a_layers[] = {
			{green_background, NULL},
			{fade_in_a, NULL},
			{NULL, NULL},
		};
		const draw_element_t draw_alphabet_layers[] = {
			{green_background, NULL},
			{draw_alphabet, NULL},
			{NULL, NULL},
		};
		const draw_element_t shrink_a_layers[] = {
			{green_background, NULL},
			{shrink_a, NULL},
			{NULL, NULL},
		};
		const draw_element_t perfect_rendering_layers[] = {
			{green_background, NULL},
			{perfect_rendering, NULL},
			{NULL, NULL},
		};
		const draw_element_t fade_out_green_layers[] = {
			{fade_out_green, NULL},
			{NULL, NULL},
		};
		const draw_element_t complex_text_demo_layers[] = {
			{complex_text_demo, NULL},
			{NULL, NULL},
		};
		const animation_step_t animation[] = {
			{ 60, fade_in_green_layers },
			{ 60, fade_in_a_layers },
			{ 600, draw_alphabet_layers },
			{ 20, noop_layers },
			{ 60, shrink_a_layers },
			{ 300, perfect_rendering_layers },
			{ 60, fade_out_green_layers },
			{ 4000, complex_text_demo_layers },
			{ 600, gradient_layers },
			{ 0, NULL },
		};

		const animation_step_t *animation_step = animation;
		draw_event_param_t draw_state = {
			.frame = 0,
			.total_frame = 0,
			.duration = 0,
			.user_data = NULL,
		};

		while (animation_step->draw_elements) {
			const draw_element_t *current_layer;

			draw_state.frame = 0;
			draw_state.duration = animation_step->duration;

			// Before draw calls
			current_layer = animation_step->draw_elements;
			while (current_layer->callback) {
				draw_state.user_data = current_layer->user_data;
				current_layer->callback(&display, DRAW_EVENT_START, &draw_state);
				current_layer++;
			}

			while (draw_state.frame < animation_step->duration) {
				// Before frame
				current_layer = animation_step->draw_elements;
				bool has_render_layer = (bool)current_layer->callback;
				while (current_layer->callback) {
					draw_state.user_data = current_layer->user_data;
					current_layer->callback(&display, DRAW_EVENT_FRAME_START, &draw_state);
					current_layer++;
				}

				if (has_render_layer) {
					uint32_t ticks_before_frame = esp_cpu_get_ccount();
					st7789_randomize_dither_table();
					for (size_t block = 0; block < ST7789_DISPLAY_WIDTH; block += ST7789_BUFFER_SIZE) {
						current_layer = animation_step->draw_elements;
						while (current_layer->callback) {
							draw_state.user_data = current_layer->user_data;
							current_layer->callback(&display, block, &draw_state);
							current_layer++;
						}
						st7789_swap_buffers(&display);
					}
					uint32_t ticks_after_frame = esp_cpu_get_ccount();
					printf("\rf: %08d, time: %.4f", (int)draw_state.total_frame, ((double)ticks_after_frame - (double)ticks_before_frame) / 240000.0);
				}
				else {
					vTaskDelay(1000 / 40 / portTICK_PERIOD_MS);
				}

				// After frame
				current_layer = animation_step->draw_elements;
				while (current_layer->callback) {
					draw_state.user_data = current_layer->user_data;
					current_layer->callback(&display, DRAW_EVENT_FRAME_END, &draw_state);
					current_layer++;
				}

				draw_state.frame++;
				draw_state.total_frame++;
			}

			// After draw calls
			current_layer = animation_step->draw_elements;
			while (current_layer->callback) {
				draw_state.user_data = current_layer->user_data;
				current_layer->callback(&display, DRAW_EVENT_END, &draw_state);
				current_layer++;
			}

			animation_step++;
		}

		font_face_destroy(&font_face);
	}

	// Clean
	vTaskDelay(portMAX_DELAY);
	vTaskDelete(NULL);
}
