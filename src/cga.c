//
// Created by xrip on 30.10.2023.
//
#include "cga.h"

uint8_t cursor_blink_state = 0;

const uint32_t cga_palette[16] = { //R, G, B
         0x000000 , //black
         0x0000AA , //blue
         0x00AA00 , //green
         0x00AAAA , //cyan
         0xAA0000 , //red
         0xAA00AA , //magenta
         0xAA5500 , //brown
         0xAAAAAA , //light gray
         0x555555 , //dark gray
         0x5555FF , //light blue
         0x55FF55 , //light green
         0x55FFFF , //light cyan
         0xFF5555 , //light red
         0xFF55FF , //light magenta
         0xFFFF55 , //yellow
         0xFFFFFF   //white
};

const uint32_t cga_composite_palette[16] = { //R, G, B
        0x000000 , // black
        0x006300 , // d.green
        0x0042E2 , // d.blue
        0x009FFD , // m.blue
        0xA6005E , // red
        0x77737A , // gray
        0xD14DFF , // purple
        0x99ACFF , // l.blue
        0x4D4000 , // brown
        0x00B900 , // l.green
        0x77737A , // gray 2
        0x00EB91 , // aqua
        0xFF4400 , // orange
        0xDFC400 , // yellow
        0xFF85F0 , // pink
        0xFFFCFF   //white
};