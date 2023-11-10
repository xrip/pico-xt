#pragma once

#include "inttypes.h"
#include "stdbool.h"

#define PIO_VGA (pio0)
#define beginVGA_PIN (6)
#define VGA_DMA_IRQ (DMA_IRQ_0)

enum graphics_mode_t {
    TEXTMODE_40x30,
    TEXTMODE_80x30,
    TEXTMODE_160x100,

    CGA_160x200x16,
    CGA_320x200x4,
    CGA_640x200x2,

    VGA_640x480x256_DIV_2,
    VGA_640x480x256_DIV_3,
};


void graphics_init();

void graphics_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height);

void graphics_set_textbuffer(uint8_t *buffer);

void graphics_set_offset(int x, int y);

void graphics_set_mode(enum graphics_mode_t mode);

void graphics_set_flashmode(bool flash_line, bool flash_frame);

void graphics_set_palette(uint8_t i_color, uint32_t color888);

void graphics_set_bgcolor(uint32_t color888);

void clrScr(uint8_t color);

void draw_text(char *string, int x, int y, uint8_t color, uint8_t bgcolor);