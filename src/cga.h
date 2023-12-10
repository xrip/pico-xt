//
// Created by xrip on 30.10.2023.
//
#pragma once

#ifndef PICO_XT_CGA_H
#define PICO_XT_CGA_H

#include <stdint.h>
#include <stdbool.h>

#ifndef PSRAM_ONLY_NO_RAM
extern uint8_t RAM[];
#endif
extern uint8_t cursor_blink_state, cga_intensity, cga_colorset;
extern const uint8_t cga_gfxpal[3][2][4];
extern const uint32_t cga_palette[16];
extern const uint32_t cga_grayscale_palette[16];
extern uint32_t vga_palette[256];
extern uint32_t cga_composite_palette[3][16];
extern uint32_t tandy_palette[16];

#ifndef PSRAM_ONLY_NO_RAM
#define CURSOR_X RAM[0x450]
#define CURSOR_Y RAM[0x451]
#else
#define CURSOR_X -1
#define CURSOR_Y -1
#endif

#endif //PICO_XT_CGA_H
