#pragma once

#include "inttypes.h"
#include "stdbool.h"


enum VGA_mode_t{VGA640x480div2,VGA640x480div3,VGA640x480_text_80_30};

#define PIO_VGA (pio0)
#define beginVGA_PIN (6)
#define VGA_DMA_IRQ (DMA_IRQ_0)

void initVGA();
void setVGAbuf(uint8_t* buf, uint16_t w,uint16_t h);

void setVGA_text_buf(uint8_t* buf_text);



void setVGAbuf_pos(int x,int y);
void setVGAmode(enum VGA_mode_t mode_VGA);

void setVGA_color_flash_mode(bool flash_line,bool flash_frame);
void setVGA_color_palette(uint8_t i_color, uint32_t color888);
void setVGA_color_palette_222(uint8_t i_color, uint32_t color888);

void setVGA_bg_color( uint32_t color888);


void clrScr(uint8_t color);
void draw_text(char *string, int x, int y, uint8_t color, uint8_t bgcolor) ;