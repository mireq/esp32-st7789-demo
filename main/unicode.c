// SPDX-License-Identifier: MIT
#include "unicode.h"


uint8_t u8_encode(char *str, uint32_t ucode) {
	if (ucode < 0x80) {
		str[0] = (char)(ucode & 0xff);
		return 1;
	}
	else if (ucode < 0x800) {
		str[0] = (char)(((ucode >> 6) & 0x1f) | 0xc0);
		str[1] = (char)(((ucode >> 0) & 0x3f) | 0x80);
		return 2;
	}
	else if (ucode < 0x10000) {
		str[0] = (char)(((ucode >> 12) & 0x0f) | 0xe0);
		str[1] = (char)(((ucode >> 6) & 0x3f) | 0x80);
		str[2] = (char)(((ucode >> 0) & 0x3f) | 0x80);
		return 3;
	}
	else if (ucode < 0x110000) {
		str[0] = (char)(((ucode >> 18) & 0x07) | 0xf0);
		str[1] = (char)(((ucode >> 12) & 0x3f) | 0x80);
		str[2] = (char)(((ucode >> 6) & 0x3f) | 0x80);
		str[3] = (char)(((ucode >> 0) & 0x3f) | 0x80);
		return 4;
	}
	else {
		// Error
		return 0;
	}
}

uint8_t u8_decode(uint32_t *ucode, const char *str) {
	*ucode = 0;
	if (*str == 0) {
		return 0;
	}
	else if (*str < 0x80) {
		*ucode = *str;
		return 1;
	}
	else if (*str < 0xe0) {
		*ucode = *str & 0x1f;
		*ucode = (*ucode << 6) | (*++str & 0x3f);
		return 2;
	}
	else if (*str < 0xf0) {
		*ucode = *str & 0x0f;
		*ucode = (*ucode << 6) | (*++str & 0x3f);
		*ucode = (*ucode << 6) | (*++str & 0x3f);
		return 3;
	}
	else if (*str < 0xf5) {
		*ucode = *str & 0x07;
		*ucode = (*ucode << 6) | (*++str & 0x3f);
		*ucode = (*ucode << 6) | (*++str & 0x3f);
		*ucode = (*ucode << 6) | (*++str & 0x3f);
		return 4;
	}
	return 0;
}
