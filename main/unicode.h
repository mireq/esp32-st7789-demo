// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>


// Encode utf code and saves to str, returns length of utf-8 string
uint8_t u8_encode(char *str, uint32_t ucode);
// Decode utf code and saves to ucode, returns length of utf-8 string
uint8_t u8_decode(uint32_t *ucode, const char *str);
