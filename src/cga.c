//
// Created by xrip on 30.10.2023.
//
#include "cga.h"

uint8_t cursor_blink_state = 0;
uint8_t cga_intensity = 0;
uint8_t cga_colorset = 0;

const uint32_t cga_palette[16] = { //R, G, B
        0x000000, //black
        0x0000C4, //blue
        0x00C400, //green
        0x00C4C4, //cyan
        0xC40000, //red
        0xC400C4, //magenta
        0xC47E00, //brown
        0xC4C4C4, //light gray
        0x4E4E4E, //dark gray
        0x4E4EDC, //light blue
        0x4EDC4E, //light green
        0x4EF3F3, //light cyan
        0xDC4E4E, //light red
        0xF34EF3, //light magenta
        0xF3F34E, //yellow
        0xFFFFFF   //white
};

const uint8_t cga_gfxpal[3][2][4] = { //palettes for 320x200 graphics mode
        {
                { 0, 2,  4,  6 }, //normal palettes
                { 0, 3,  5,  7 }
        },
        {
                { 0, 10, 12, 14 }, //intense palettes
                { 0, 11, 13, 15 }
        },
        { // the unofficial Mode 5 palette, accessed by disabling ColorBurst
                {0, 3, 4, 7 },
                {0, 11,12, 15},
        },
};

const uint32_t cga_composite_palette[3][16] = { //R, G, B
        // 640x200 Color Composite
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
        { // 320x200 Palette 0 High Intensity Composite Color
                0x000000, //
                0x00766d, //
                0x00316f, //
                0x7b3400, //
                0x39be42, //
                0x837649, //
                0x539b0e, //
                0xeb3207, //
                0xd2c499, //
                0xf87a9b, //
                0xd9a06b, //
                0xb34400, //
                0x8bd04a, //
                0xbe8550, //
                0x98ad14, //
                0x000000  // ???
        },
        { // 320x200 Palette 1 High Intensity Composite Color
                0x000000, //
                0x008bac, //
                0x0049ae, //
                0x009ee8, //
                0x581c00, //
                0x00bc9b, //
                0x64759f, //
                0x00cdd9, //
                0xc81b26, //
                0xb2c2ec, //
                0xdd7def, //
                0xbed3ff, //
                0xff4900, //
                0xf6edc0, //
                0xffa4c3, //
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