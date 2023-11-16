//
// Created by xrip on 30.10.2023.
//
#pragma once

#ifndef PICO_XT_CGA_H
#define PICO_XT_CGA_H

#include <stdint.h>
#include <stdbool.h>

extern uint8_t cursor_blink_state, cga_intensity, cga_colorset;
extern const uint8_t cga_gfxpal[2][2][4];
extern const uint32_t cga_palette[16];
extern const uint32_t cga_composite_palette[3][16];
extern const uint32_t tandy_palette[16];

uint8_t read86(uint32_t addr32);
#define CURSOR_X read86(0x450)
#define CURSOR_Y read86(0x451)

#endif //PICO_XT_CGA_H
