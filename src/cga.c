//
// Created by xrip on 30.10.2023.
//
#include "cga.h"

uint8_t cursor_blink_state = 0;
uint8_t cga_intensity = 0;
uint8_t cga_colorset = 0;

const uint32_t cga_palette[16] = { //R, G, B
        0x000000, //black
        0x0000AA, //blue
        0x00AA00, //green
        0x00AAAA, //cyan
        0xAA0000, //red
        0xAA00AA, //magenta
        0xAA5500, //brown
        0xAAAAAA, //light gray
        0x555555, //dark gray
        0x5555FF, //light blue
        0x55FF55, //light green
        0x55FFFF, //light cyan
        0xFF5555, //light red
        0xFF55FF, //light magenta
        0xFFFF55, //yellow
        0xFFFFFF   //white
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

const uint32_t cga_composite_palette[3][16] = { //R, G, B
        // 640x200
        {
                0x000000, // black
                0x007100, // d.green
                0x003fff, // d.blue
                0x00abff, // m.blue
                0xc10065, // red
                0x737373, // gray
                0xe639ff, // purple
                0x8ca8ff, // l.blue
                0x554600, // brown
                0x00cd00, // l.green
                0x00cd00, // gray 2
                0x00fc7e, // aqua
                0xff3900, // orange
                0xe4cc00, // yellow
                0xff7af2, // pink
                0xffffff  //white
        },
        { // 320x200 pal 1
                0x000000, //
                0x00b5ff, //
                0x0073ff, //
                0x00ceff, //
                0xff0000, //
                0x23d9b9, //
                0xb98fa5, //
                0x00f8ff, //
                0xff0000, //
                0x71c6ff, //
                0xdd7def, //
                0x36e0ff, //
                0xff0400, //
                0xffdf9c, //
                0xff9489, //
                0xffffff  //
        },
        { // 320x200 pal 2
                0x000000, //
                0x009ee3, //
                0x005ad0, //
                0x00a5f6, //
                0xff0800, //
                0x45c84c, //
                0xc07d38, //
                0x37cd61, //
                0xff0000, //
                0x83b19b, //
                0xe36687, //
                0x7bb7ae, //
                0xff0d00, //
                0xc6ce2b, //
                0xff810e, //
                0xffffff  //
        }
};

const uint32_t tandy_palette[16] = { //R, G, B
        0x000000,
        0x0000aa,
        0x00aa00,
        0x00aaaa,
        0xaa0000,
        0xaa00aa,
        0xaa5500,
        0xaaaaaa,
        0x555555,
        0x5555ff,
        0x55ff55,
        0x55ffff,
        0xff5555,
        0xff55ff,
        0xffff55,
        0xffffff,
};