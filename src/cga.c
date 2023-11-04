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
