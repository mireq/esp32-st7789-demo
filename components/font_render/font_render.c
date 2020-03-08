// SPDX-License-Identifier: MIT

#include "esp_log.h"
#include "esp_heap_caps.h"

#include "font_render.h"


static FT_Library ft_library = NULL;
static const char *TAG = "font_render";


#ifndef FONT_CACHE_ALLOC
#define FONT_CACHE_ALLOC MALLOC_CAP_DEFAULT
#endif


static void font_cache_destroy(font_render_t *render) {
	if (render->glyph_cache_records) {
		heap_caps_free(render->glyph_cache_records);
		render->glyph_cache_records = NULL;
	}
	if (render->glyph_cache) {
		heap_caps_free(render->glyph_cache);
		render->glyph_cache = NULL;
	}
}


static esp_err_t font_cache_init(font_render_t *render) {
	font_cache_destroy(render);

	render->max_pixel_width = (render->pixel_size * (render->font_face->ft_face->bbox.xMax - render->font_face->ft_face->bbox.xMin)) / render->font_face->ft_face->units_per_EM + 1;
	render->max_pixel_height = (render->pixel_size * (render->font_face->ft_face->bbox.yMax - render->font_face->ft_face->bbox.yMin)) / render->font_face->ft_face->units_per_EM + 1;
	render->bytes_per_glyph = (size_t)render->max_pixel_width * (size_t)render->max_pixel_height * 2;
	render->bytes_per_glyph = (render->bytes_per_glyph >> 3) + (render->bytes_per_glyph & 0x07 ? 1 : 0);

	render->glyph_cache = (uint8_t *)heap_caps_malloc(render->bytes_per_glyph * render->cache_size, FONT_CACHE_ALLOC);
	render->glyph_cache_records = (glyph_cache_record_t *)heap_caps_malloc(sizeof(glyph_cache_record_t) * render->cache_size, FONT_CACHE_ALLOC);

	if (!render->glyph_cache || !render->glyph_cache_records) {
		ESP_LOGE(TAG, "Glyph cache not allocated");
		if (render->glyph_cache_records) {
			heap_caps_free(render->glyph_cache_records);
			render->glyph_cache_records = NULL;
		}
		if (render->glyph_cache) {
			heap_caps_free(render->glyph_cache);
			render->glyph_cache = NULL;
		}
		return ESP_FAIL;
	}

	memset(render->glyph_cache_records, 0, sizeof(glyph_cache_record_t) * render->cache_size);

	return ESP_OK;
}


esp_err_t font_face_init(font_face_t *face, const font_data_t *data, font_data_size_t size) {
	FT_Error err;

	face->pixel_size = 0;

	if (ft_library == NULL) {
		err = FT_Init_FreeType(&ft_library);
		if (err) {
			ESP_LOGE(TAG, "Freetype not loaded: %d", err);
			return ESP_FAIL;
		}
	}

	err = FT_New_Memory_Face(ft_library, data, size, 0, &face->ft_face);
	if (err) {
		ESP_LOGE(TAG, "New face failed: %d", err);
		return ESP_FAIL;
	}

	return ESP_OK;
}

void font_face_destroy(font_face_t *face) {
	FT_Done_Face(face->ft_face);
}


esp_err_t font_face_set_pixel_size(font_face_t *face, font_size_t pixel_size) {
	if (face->pixel_size != pixel_size) {
		FT_Error err = FT_Set_Pixel_Sizes(face->ft_face, 0, pixel_size);
		if (err) {
			ESP_LOGE(TAG, "Set font size failed: %d", err);
			return ESP_FAIL;
		}
	}
	return ESP_OK;
}


esp_err_t font_render_init(font_render_t *render, font_face_t *face, font_size_t pixel_size, uint16_t cache_size) {
	render->font_face = face;
	render->glyph_cache = NULL;
	render->glyph_cache_records = NULL;
	render->pixel_size = pixel_size;
	render->cache_size = cache_size;

	if (font_face_set_pixel_size(face, pixel_size) != ESP_OK) {
		return ESP_FAIL;
	}

	if (font_cache_init(render) != ESP_OK) {
		return ESP_FAIL;
	}

	return ESP_OK;
}


void font_render_destroy(font_render_t *render) {
	font_cache_destroy(render);
}


esp_err_t font_load_glyph_metrics(font_render_t *render, uint32_t utf_code) {
	if (font_face_set_pixel_size(render->font_face, render->pixel_size) != ESP_OK) {
		return ESP_FAIL;
	}

	FT_UInt glyph_index = FT_Get_Char_Index(render->font_face->ft_face, utf_code);
	if (glyph_index == 0) {
		return ESP_FAIL;
	}

	FT_Error err;
	err = FT_Load_Glyph(render->font_face->ft_face, glyph_index,  FT_LOAD_DEFAULT);
	if (err) {
		return ESP_FAIL;
	}

	render->metrics = render->font_face->ft_face->glyph->metrics;

	return ESP_OK;
}


esp_err_t font_render_glyph(font_render_t *render, uint32_t utf_code) {
	// Search cache entry
	int found_cache = -1;
	for (size_t i = 0; i < render->cache_size; ++i) {
		if (render->glyph_cache_records[i].utf_code == utf_code) {
			found_cache = i;
		}
	}

	if (found_cache == -1) {
		if (font_face_set_pixel_size(render->font_face, render->pixel_size) != ESP_OK) {
			return ESP_FAIL;
		}

		FT_UInt glyph_index = FT_Get_Char_Index(render->font_face->ft_face, utf_code);
		if (glyph_index == 0) {
			return ESP_FAIL;
		}

		FT_Error err;
		err = FT_Load_Glyph(render->font_face->ft_face, glyph_index,  FT_LOAD_DEFAULT);
		if (err) {
			return ESP_FAIL;
		}

		err = FT_Render_Glyph(render->font_face->ft_face->glyph, FT_RENDER_MODE_NORMAL);
		if (err) {
			ESP_LOGE(TAG, "Glyph not rendered %d", err);
			return ESP_FAIL;
		}

		// Find lowest priority
		uint16_t min_priority = 0xffff;
		for (size_t i = 0; i < render->cache_size; ++i) {
			if (render->glyph_cache_records[i].priority <= min_priority) {
				min_priority = render->glyph_cache_records[i].priority;
				found_cache = i;
			}
		}

		render->glyph_cache_records[found_cache].utf_code = utf_code;
		render->glyph_cache_records[found_cache].bitmap_width = render->font_face->ft_face->glyph->bitmap.width;
		render->glyph_cache_records[found_cache].bitmap_height = render->font_face->ft_face->glyph->bitmap.rows;
		render->bitmap = render->glyph_cache + (render->bytes_per_glyph * found_cache);
		memset(render->bitmap, 0, render->bytes_per_glyph);
		size_t pos = 0;
		for (size_t y = 0; y < render->font_face->ft_face->glyph->bitmap.rows; ++y) {
			for (size_t x = 0; x < render->font_face->ft_face->glyph->bitmap.width; ++x) {
				uint8_t color = render->font_face->ft_face->glyph->bitmap.buffer[y * render->font_face->ft_face->glyph->bitmap.pitch + x];
				uint8_t compressed_color = 0;
				if (color >= 160) {
					compressed_color = 3;
				}
				else if (color >= 96) {
					compressed_color = 2;
				}
				else if (color >= 32) {
					compressed_color = 1;
				}
				//render->bitmap[pos >> 2] |= ((color >> 6) << ((pos & 0x03) << 1));
				render->bitmap[pos >> 2] |= compressed_color << ((pos & 0x03) << 1);
				pos++;
			}
		}
	}


	for (size_t i = 0; i < render->cache_size; ++i) {
		render->glyph_cache_records[i].priority--;
	}
	render->glyph_cache_records[found_cache].priority = 0xffff;
	render->metrics = render->glyph_cache_records[found_cache].metrics;
	render->bitmap_width = render->glyph_cache_records[found_cache].bitmap_width;
	render->bitmap_height = render->glyph_cache_records[found_cache].bitmap_height;
	render->bitmap = render->glyph_cache + (render->bytes_per_glyph * found_cache);

	return ESP_OK;
}
