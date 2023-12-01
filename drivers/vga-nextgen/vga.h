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

    TGA_320x200x16,
    EGA_320x200x16,
    VGA_320x200x256,
};


void graphics_init();

void graphics_set_buffer(uint8_t *buffer, uint16_t width, uint16_t height);

void graphics_set_textbuffer(uint8_t *buffer);

void graphics_set_offset(int x, int y);

enum graphics_mode_t graphics_set_mode(enum graphics_mode_t mode);

void graphics_set_flashmode(bool flash_line, bool flash_frame);

void graphics_set_palette(uint8_t i, uint32_t color888);

void graphics_set_bgcolor(uint32_t color888);

void clrScr(uint8_t color);

void draw_text(char *string, int x, int y, uint8_t color, uint8_t bgcolor);

void logMsg(char * msg);

void set_start_debug_line(int _start_debug_line);

char* get_free_vram_ptr();
