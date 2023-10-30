#pragma once
#ifndef BIOS_H
#define BIOS_H

#include <memory.h>
#include "stdbool.h"
#include "vc.h"
#include "cpu8086.h"
#include "disk.h"
#include "ports.h"

#if PICO_ON_DEVICE
#endif

/*

int color = 7;


#define CURX RAM[0x450]
#define CURY RAM[0x451]
static void bios_putchar(const char c) {

    //printf("\033[%im%c", color, c);
    if (c == 0x0D) {
        CURX = 0;
        CURY++;
    } else if (c == 0x0A) {
        CURX = 0;
    } else if (c == 0x08 && CURX > 0) {
        CURX++;
        VRAM[*/
/*0xB8000 + *//*
(CURY * 160) + CURX * 2 + 0] = 32;
        VRAM[*/
/*0xB8000 + *//*
(CURY * 160) + CURX * 2 + 1] = color;
    } else {
        VRAM[*/
/*0xB8000 + *//*
(CURY * 160) + CURX * 2 + 0] = c & 0xFF;
        VRAM[*/
/*0xB8000 + *//*
(CURY * 160) + CURX * 2 + 1] = color;
        if (CURX == 79) {
            CURX = 0;
            CURY++;
        } else
            CURX++;
    }

    if (CURY == 25) {
        CURY = 24;

        memmove(VRAM*/
/* + 0xB8000*//*
, VRAM */
/*+ 0xB8000*//*
 + 160, 80 * 25 * 2);
        for (int a = 0; a < 80; a++) {
            VRAM[*/
/*0xB8000 + *//*
24 * 160 + a * 2 + 0] = 32;
            VRAM[*/
/*0xB8000 + *//*
24 * 160 + a * 2 + 1] = color;

        }
    }
}

static void bios_putstr(const char *s) {
    while (*s)
        bios_putchar(*s++);
}

#define bios_printf(...)                    \
    do {                            \
        char _buf_[4096];                \
        snprintf(_buf_, sizeof(_buf_), __VA_ARGS__);    \
        bios_putstr(_buf_);                \
    } while (0)

*/

int cpu_hlt_handler(void) {
    puts("BIOS: critical warning, HLT outside of trap area?!\r\n");
    return 1;    // Yes, it was really a halt, since it does not fit into our trap area
}


#endif