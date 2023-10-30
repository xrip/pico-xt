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
#include "ps2.h"
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

#if PICO_ON_DEVICE
#define PS2_KEYMAP_SIZE 136
typedef struct {
    uint8_t noshift[PS2_KEYMAP_SIZE];
    uint8_t shift[PS2_KEYMAP_SIZE];
    uint8_t uses_altgr;
    uint8_t altgr[PS2_KEYMAP_SIZE];
} PS2Keymap_t;

extern const PS2Keymap_t PS2Keymap_US;

const PS2Keymap_t PS2Keymap_US = {
        // without shift
        {0, 0x43, 0, 0x3F, 0x3D, 0x3B, 0x3C, 0 /*PS2_F12*/,
         0, 0x44, 0x42, 0x40, 0x3E, 0x0F, 0x29, 0,
         0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 0x10, 0x02, 0,
         0, 0, 0x2C, 0x1F, 0x1E, 0x11, 0x03, 0,
         0, 0x2E, 0x2D, 0x20, 0x12, 0x05, 0x04, 0,
         0, 0x39, 0x2F, 0x21, 0x14, 0x13, 0x06, 0,
         0, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0,
         0, 0, 0x32, 0x24, 0x16, 0x08, 0x09, 0,
         0, 0x33, 0x25, 0x17, 0x18, 0x0B, 0x0A, 0,
         0, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0C, 0,
         0, 0, 0x28, 0, 0x1A, 0x0D, 0, 0,
         0 /*CapsLock*/, 0 /*Rshift*/, 0x1C, 0x1B, 0, 0x2B, 0, 0,
         0, 0, 0, 0, 0, 0, 0x0E, 0,
         0, 0xcf /*Gray 1*/, 0, 0xcb /*Gray 4*/, 0xc7 /*Gray 7*/, 0, 0, 0,
         0xd2 /*Gray 0*/, 0x34, 0xd0 /*Gray 2*/, 0xcc /*Gray 5*/, 0xcd /*Gray 6*/, 0xc8 /*Gray 8*/, 0x01, 0 /*NumLock*/,
         0 /*PS2_F11*/, 0x4E, 0xd1 /*Gray 3*/, 0x4A, 0x37, 0xc9 /*Gray 9*/, 0 /*PS2_SCROLL*/, 0,
         0, 0, 0, 0x41},

        // with shift
        {0, 0x5C, 0, 0x58, 0x56, 0x54, 0x55, 0 /*PS2_F12*/,
         0, 0x5D, 0x5B, 0x59, 0x57, 0x0F, 0x29, 0,
         0, 0 /*Lalt*/, 0 /*Lshift*/, 0, 0 /*Lctrl*/, 0x10, 0x02, 0,
         0, 0, 0x2C, 0x1F, 0x1E, 0x11, 0x03, 0,
         0, 0x2E, 0x2D, 0x20, 0x12, 0x05, 0x04, 0,
         0, 0x39 /*sh+space*/, 0x2F, 0x21, 0x14, 0x13, 0x06, 0,
         0, 0x31, 0x30, 0x23, 0x22, 0x15, 0x07, 0,
         0, 0, 0x32, 0x24, 0x16, 0x08, 0x09, 0,
         0, 0x33, 0x25, 0x17, 0x18, 0x0B, 0x0A, 0,
         0, 0x34, 0x35, 0x26, 0x27, 0x19, 0x0C, 0,
         0, 0, 0x28, 0, 0x1A, 0x0D, 0, 0,
         0 /*CapsLock*/, 0 /*Rshift*/, 0x1C /*Enter*/, 0x1B, 0, 0x2B, 0, 0,
         0, 0, 0, 0, 0, 0, 0x0E, 0,
         0, '1', 0, '4', '7', 0, 0, 0,
         '0', '.', '2', '5', '6', '8', 0x01, 0 /*NumLock*/,
         0 /*PS2_F11*/, 0x0D, '3', 0x0C, 0x37, '9', 0 /*PS2_SCROLL*/, 0,
         0, 0, 0, 0x5A },
        0
};
#define BREAK     0x01
#define MODIFIER  0x02
#define SHIFT_L   0x04
#define SHIFT_R   0x08
#define ALTGR     0x10
#define CTRL      0x20


static char get_scancode(void) {
    static uint8_t state=0;
    uint8_t s;
    char c;

    while (1) {
        s = get_scan_code();
        if (!s) return 0;
        if (s == 0xF0) {
            state |= BREAK;
            return 0x80;                      // retrun break??
        } else if (s == 0xE0) {
            state |= MODIFIER;
        } else {
            if (state & BREAK) {
                if (s == 0x12) {                    ///0x12 is left shift key pressed
                    state &= ~SHIFT_L;
                } else if (s == 0x59) {
                    state &= ~SHIFT_R;                /// 0x59 is right shift
                } else if (s == 0x14) {             /// 0x14 is ctrl key pressed
                    state &= ~CTRL;
                } else if (s == 0x11) {             ///0x11 is alt key pressed
                    state &= ~ALTGR;
                }
                state &= ~(BREAK | MODIFIER);
                continue;
            }
            if (s == 0x12) {
                state |= SHIFT_L;
                continue;
            } else if (s == 0x59) {
                state |= SHIFT_R;
                continue;
            } else if (s == 0x14) {
                state |= CTRL;
                continue;
            } else if (s == 0x11) {
                state |= ALTGR;
                continue;
            }
            c = 0;
            if (state & MODIFIER) {
                switch (s) {
                    case 0x70: c = 0x52; break;
                    case 0x6C: c = 0x47; break;
                    case 0x7D: c = 0x49; break;
                    case 0x71: c = 0x53; break;
                    case 0x69: c = 0x4F; break;
                    case 0x7A: c = 0x51; break;
                    case 0x75: c = 0x48; break;
                    case 0x6B: c = 0x4B; break;
                    case 0x72: c = 0x50; break;
                    case 0x74: c = 0x4D; break;
                    case 0x4A: c = 0x35; break;
                    case 0x5A: c = 0x1C; break;
                    default: break;
                }
            } else if (state & (SHIFT_L | SHIFT_R)) {
                if (s < PS2_KEYMAP_SIZE)
                    c = PS2Keymap_US.shift[s];
            } else {
                if (s < PS2_KEYMAP_SIZE)
                    c = PS2Keymap_US.noshift[s];
            }

            state &= ~(BREAK | MODIFIER);

            switch (state) {
                case 0x04: RAM[0x417]= 0x02; break;
                case 0x08: RAM[0x417]= 0x01; break;
                case 0x0c: RAM[0x417]= 0x03; break;
                case 0x10: RAM[0x417]= 0x08; break;
                case 0x14: RAM[0x417]= 0x0a; break;
                case 0x18: RAM[0x417]= 0x09; break;
                case 0x1c: RAM[0x417]= 0x0b; break;
                case 0x20: RAM[0x417]= 0x04; break;
                case 0x24: RAM[0x417]= 0x06; break;
                case 0x28: RAM[0x417]= 0x05; break;
                case 0x2c: RAM[0x417]= 0x07; break;
                case 0x30: RAM[0x417]= 0x0c; break;
                case 0x34: RAM[0x417]= 0x0e; break;
                case 0x38: RAM[0x417]= 0x0d; break;
                case 0x3c: RAM[0x417]= 0x0f; break;
                default:   RAM[0x417]= 0x00; break;
            }

            if ((c == 0x53) && (RAM[0x417]&0x0c)) {
                c =0x34;
            }

            if (c > 127) {
                c = c - 0x80;
                RAM[0x417] |= 0x20;
            }
            if (c) {
                portram[0x60] = c;
                return c;
            }
        }
    }
}

static uint8_t CharBuffer=0;
static uint8_t UTF8next=0;

static bool keyboard_available() {
    if (CharBuffer || UTF8next) return true;
    CharBuffer = get_scancode();
    if (CharBuffer) return true;
    return false;
}


int keyboard_read() {
    uint8_t result;

    result = UTF8next;
    if (result) {
        UTF8next = 0;
    } else {
        result = CharBuffer;
        if (result) {
            CharBuffer = 0;
        } else {
            result = get_scancode();
        }
    }
    if (!result) return -1;

    return result;
}
#endif
#endif