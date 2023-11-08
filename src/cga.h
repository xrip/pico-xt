//
// Created by xrip on 30.10.2023.
//
#pragma once

#ifndef PICO_XT_CGA_H
#define PICO_XT_CGA_H

#include <stdint.h>
#include <stdbool.h>

extern uint8_t RAM[];
extern uint8_t cursor_blink_state;
extern const uint32_t cga_palette[16];
extern const uint32_t cga_composite_palette[16];

#define CURX RAM[0x450]
#define CURY RAM[0x451]

#endif //PICO_XT_CGA_H
