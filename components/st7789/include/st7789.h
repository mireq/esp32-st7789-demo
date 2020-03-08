// SPDX-License-Identifier: MIT

#pragma once


#include "driver/spi_master.h"
#include "esp_err.h"


#define ST7789_SPI_QUEUE_SIZE 2


// System Function Command Table 1
#define ST7789_CMD_NOP               0x00 // No operation
#define ST7789_CMD_SWRESET           0x01 // Software reset
#define ST7789_CMD_RDDID             0x04 // Read display ID
#define ST7789_CMD_RDDST             0x09 // Read display status
#define ST7789_CMD_RDDPM             0x0a // Read display power
#define ST7789_CMD_RDDMADCTL         0x0b // Read display
#define ST7789_CMD_RDDCOLMOD         0x0c // Read display pixel
#define ST7789_CMD_RDDIM             0x0d // Read display image
#define ST7789_CMD_RDDSM             0x0e // Read display signal
#define ST7789_CMD_RDDSDR            0x0f // Read display self-diagnostic result
#define ST7789_CMD_SLPIN             0x10 // Sleep in
#define ST7789_CMD_SLPOUT            0x11 // Sleep out
#define ST7789_CMD_PTLON             0x12 // Partial mode on
#define ST7789_CMD_NORON             0x13 // Partial off (Normal)
#define ST7789_CMD_INVOFF            0x20 // Display inversion off
#define ST7789_CMD_INVON             0x21 // Display inversion on
#define ST7789_CMD_GAMSET            0x26 // Gamma set
#define ST7789_CMD_DISPOFF           0x28 // Display off
#define ST7789_CMD_DISPON            0x29 // Display on
#define ST7789_CMD_CASET             0x2a // Column address set
#define ST7789_CMD_RASET             0x2b // Row address set
#define ST7789_CMD_RAMWR             0x2c // Memory write
#define ST7789_CMD_RAMRD             0x2e // Memory read
#define ST7789_CMD_PTLAR             0x30 // Partial start/end address set
#define ST7789_CMD_VSCRDEF           0x33 // Vertical scrolling definition
#define ST7789_CMD_TEOFF             0x34 // Tearing line effect off
#define ST7789_CMD_TEON              0x35 // Tearing line effect on
#define ST7789_CMD_MADCTL            0x36 // Memory data access control
#define ST7789_CMD_VSCRSADD          0x37 // Vertical address scrolling
#define ST7789_CMD_IDMOFF            0x38 // Idle mode off
#define ST7789_CMD_IDMON             0x39 // Idle mode on
#define ST7789_CMD_COLMOD            0x3a // Interface pixel format
#define ST7789_CMD_RAMWRC            0x3c // Memory write continue
#define ST7789_CMD_RAMRDC            0x3e // Memory read continue
#define ST7789_CMD_TESCAN            0x44 // Set tear scanline
#define ST7789_CMD_RDTESCAN          0x45 // Get scanline
#define ST7789_CMD_WRDISBV           0x51 // Write display brightness
#define ST7789_CMD_RDDISBV           0x52 // Read display brightness value
#define ST7789_CMD_WRCTRLD           0x53 // Write CTRL display
#define ST7789_CMD_RDCTRLD           0x54 // Read CTRL value display
#define ST7789_CMD_WRCACE            0x55 // Write content adaptive brightness control and Color enhancemnet
#define ST7789_CMD_RDCABC            0x56 // Read content adaptive brightness control
#define ST7789_CMD_WRCABCMB          0x5e // Write CABC minimum brightness
#define ST7789_CMD_RDCABCMB          0x5f // Read CABC minimum brightness
#define ST7789_CMD_RDABCSDR          0x68 // Read Automatic Brightness Control Self-Diagnostic Result
#define ST7789_CMD_RDID1             0xda // Read ID1
#define ST7789_CMD_RDID2             0xdb // Read ID2
#define ST7789_CMD_RDID3             0xdc // Read ID3

// System Function Command Table 2
#define ST7789_CMD_RAMCTRL           0xb0 // RAM Control
#define ST7789_CMD_RGBCTRL           0xb1 // RGB Control
#define ST7789_CMD_PORCTRL           0xb2 // Porch control
#define ST7789_CMD_FRCTRL1           0xb3 // Frame Rate Control 1
#define ST7789_CMD_GCTRL             0xb7 // Gate control
#define ST7789_CMD_DGMEN             0xba // Digital Gamma Enable
#define ST7789_CMD_VCOMS             0xbb // VCOM Setting
#define ST7789_CMD_LCMCTRL           0xc0 // LCM Control
#define ST7789_CMD_IDSET             0xc1 // ID Setting
#define ST7789_CMD_VDVVRHEN          0xc2 // VDV and VRH Command enable
#define ST7789_CMD_VRHS              0xc3 // VRH Set
#define ST7789_CMD_VDVSET            0xc4 // VDV Setting
#define ST7789_CMD_VCMOFSET          0xc5 // VCOM Offset Set
#define ST7789_CMD_FRCTR2            0xc6 // FR Control 2
#define ST7789_CMD_CABCCTRL          0xc7 // CABC Control
#define ST7789_CMD_REGSEL1           0xc8 // Register value selection 1
#define ST7789_CMD_REGSEL2           0xca // Register value selection 2
#define ST7789_CMD_PWMFRSEL          0xcc // PWM Frequency Selection
#define ST7789_CMD_PWCTRL1           0xd0 // Power Control 1
#define ST7789_CMD_VAPVANEN          0xd2 // Enable VAP/VAN signal output
#define ST7789_CMD_CMD2EN            0xdf // Command 2 Enable
#define ST7789_CMD_PVGAMCTRL         0xe0 // Positive Voltage Gamma Control
#define ST7789_CMD_NVGAMCTRL         0xe1 // Negative voltage Gamma Control
#define ST7789_CMD_DGMLUTR           0xe2 // Digital Gamma Look-up Table for Red
#define ST7789_CMD_DGMLUTB           0xe3 // Digital Gamma Look-up Table for Blue
#define ST7789_CMD_GATECTRL          0xe4 // Gate control
#define ST7789_CMD_PWCTRL2           0xe8 // Power Control 2
#define ST7789_CMD_EQCTRL            0xe9 // Equalize Time Control
#define ST7789_CMD_PROMCTRL          0xec // Program Control
#define ST7789_CMD_PROMEN            0xfa // Program Mode Enable
#define ST7789_CMD_NVMSET            0xfc // NVM Setting
#define ST7789_CMD_PROMACT           0xfe // Program Action

#define ST7789_CMDLIST_END           0xff // End command (used for command list)

struct st7789_driver;

typedef struct st7789_transaction_data {
	struct st7789_driver *driver;
	bool data;
} st7789_transaction_data_t;

typedef uint16_t st7789_color_t;

typedef struct st7789_driver {
	int pin_reset;
	int pin_dc;
	int pin_mosi;
	int pin_sclk;
	int spi_host;
	int dma_chan;
	uint8_t queue_fill;
	uint16_t display_width;
	uint16_t display_height;
	spi_device_handle_t spi;
	size_t buffer_size;
	st7789_transaction_data_t data;
	st7789_transaction_data_t command;
	st7789_color_t *buffer;
	st7789_color_t *buffer_a;
	st7789_color_t *buffer_b;
	st7789_color_t *current_buffer;
	spi_transaction_t trans_a;
	spi_transaction_t trans_b;
} st7789_driver_t;

typedef struct st7789_command {
	uint8_t command;
	uint8_t wait_ms;
	uint8_t data_size;
	const uint8_t *data;
} st7789_command_t;

esp_err_t st7789_init(st7789_driver_t *driver);
void st7789_reset(st7789_driver_t *driver);
void st7789_lcd_init(st7789_driver_t *driver);
void st7789_start_command(st7789_driver_t *driver);
void st7789_start_data(st7789_driver_t *driver);
void st7789_run_command(st7789_driver_t *driver, const st7789_command_t *command);
void st7789_run_commands(st7789_driver_t *driver, const st7789_command_t *sequence);
void st7789_clear(st7789_driver_t *driver, st7789_color_t color);
void st7789_fill_area(st7789_driver_t *driver, st7789_color_t color, uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height);
void st7789_fill_area(st7789_driver_t *driver, st7789_color_t color, uint16_t start_x, uint16_t start_y, uint16_t width, uint16_t height);
void st7789_set_window(st7789_driver_t *driver, uint16_t start_x, uint16_t start_y, uint16_t end_x, uint16_t end_y);
void st7789_write_pixels(st7789_driver_t *driver, st7789_color_t *pixels, size_t length);
void st7789_wait_until_queue_empty(st7789_driver_t *driver);
void st7789_swap_buffers(st7789_driver_t *driver);
st7789_color_t st7789_rgb_to_color(uint8_t r, uint8_t g, uint8_t b);
void st7789_color_to_rgb(st7789_color_t color, uint8_t *r, uint8_t *g, uint8_t *b);
void st7789_draw_gray2_bitmap(uint8_t *src_buf, st7789_color_t *target_buf, uint8_t r, uint8_t g, uint8_t b, int x, int y, int src_w, int src_h, int target_w, int target_h);
