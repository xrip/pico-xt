//
// Created by xrip on 30.10.2023.
//
#include "cga.h"
#include <stdbool.h>

uint8_t VRAM[16384];

uint8_t cursor_blink_state = 0;

const uint32_t dosColorPalette[16] = {
        0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
        0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
        0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
        0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};


const uint8_t cga_palette[16][3] = { //R, G, B
        { 0x00, 0x00, 0x00 }, //black
        { 0x00, 0x00, 0xAA }, //blue
        { 0x00, 0xAA, 0x00 }, //green
        { 0x00, 0xAA, 0xAA }, //cyan
        { 0xAA, 0x00, 0x00 }, //red
        { 0xAA, 0x00, 0xAA }, //magenta
        { 0xAA, 0x55, 0x00 }, //brown
        { 0xAA, 0xAA, 0xAA }, //light gray
        { 0x55, 0x55, 0x55 }, //dark gray
        { 0x55, 0x55, 0xFF }, //light blue
        { 0x55, 0xFF, 0x55 }, //light green
        { 0x55, 0xFF, 0xFF }, //light cyan
        { 0xFF, 0x55, 0x55 }, //light red
        { 0xFF, 0x55, 0xFF }, //light magenta
        { 0xFF, 0xFF, 0x55 }, //yellow
        { 0xFF, 0xFF, 0xFF }  //white
};

const uint8_t cga_gfxpal[2][2][4] = { //palettes for 320x200 graphics mode
        {
                { 0, 2,  4,  6 }, //normal palettes
                { 0, 3,  5,  7 }
        },
        {
                { 0, 10, 12, 14 }, //intense palettes
                { 0, 11, 13, 15 }
        }
};

