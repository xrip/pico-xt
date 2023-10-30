//
// Created by xrip on 30.10.2023.
//

#ifndef PICO_XT_CGA_H
#define PICO_XT_CGA_H
#include <stdint.h>

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

#define cga_color(c) ((uint32_t)cga_palette[c][2] | ((uint32_t)cga_palette[c][1]<<8) | ((uint32_t)cga_palette[c][0]<<16))


#endif //PICO_XT_CGA_H
