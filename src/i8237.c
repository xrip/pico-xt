/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers
            (C)2020      Gabor Lenart "LGB"

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

/* i8237.c: functions to emulate the Intel 8237 DMA controller.
   the Sound Blaster emulation functions rely on this! */

#include "emulator.h"
#define DEBUG_DMA 1

#ifdef SOUND_BLASTER
#ifndef DMA_8237
#define DMA_8237
#endif
#endif
#ifdef DMA_8237

static struct dmachan_s dmachan[4] = { 0, 0, 0, 0};
static uint8_t flipflop = 0;

// FIXME read86 is valid or we need direct ram/psram/swap acceess?
uint8_t read8237(uint8_t channel) {
    uint8_t ret;
    if (dmachan[channel].masked) return 128;
    if (dmachan[channel].autoinit && (dmachan[channel].count > dmachan[channel].reload)) dmachan[channel].count = 0;
    if (dmachan[channel].count > dmachan[channel].reload) return 128;
    //if (dmachan[channel].direction) ret = RAM[dmachan[channel].page + dmachan[channel].addr + dmachan[channel].count];
    //	else ret = RAM[dmachan[channel].page + dmachan[channel].addr - dmachan[channel].count];
    if (dmachan[channel].direction == 0)
        ret = read86(dmachan[channel].page + dmachan[channel].addr + dmachan[channel].count);
    else ret = read86(dmachan[channel].page + dmachan[channel].addr - dmachan[channel].count);
    dmachan[channel].count++;
    return ret;
}

__inline void out8237(uint16_t addr, uint8_t value) {
    uint8_t channel;
#ifdef DEBUG_DMA
    printf ("out8237(0x%X, %X);\n", addr, value);
#endif
    switch (addr) {
        case 0x2: //channel 1 address register
            if (flipflop == 1) dmachan[1].addr = (dmachan[1].addr & 0x00FF) | ((uint32_t) value << 8);
            else dmachan[1].addr = (dmachan[1].addr & 0xFF00) | value;
#ifdef DEBUG_DMA
            if (flipflop == 1) printf ("[NOTICE] DMA channel 1 address register = %04X\n", dmachan[1].addr);
#endif
            flipflop = ~flipflop & 1;
            break;
        case 0x3: //channel 1 count register
            if (flipflop == 1) dmachan[1].reload = (dmachan[1].reload & 0x00FF) | ((uint32_t) value << 8);
            else dmachan[1].reload = (dmachan[1].reload & 0xFF00) | value;
            if (flipflop == 1) {
                if (dmachan[1].reload == 0) dmachan[1].reload = 65536;
                dmachan[1].count = 0;
#ifdef DEBUG_DMA
                printf ("[NOTICE] DMA channel 1 reload register = %04X\n", dmachan[1].reload);
#endif
            }
            flipflop = ~flipflop & 1;
            break;
        case 0xA: //write single mask register
            channel = value & 3;
            dmachan[channel].masked = (value >> 2) & 1;
#ifdef DEBUG_DMA
            printf ("[NOTICE] DMA channel %u masking = %u\n", channel, dmachan[channel].masked);
#endif
            break;
        case 0xB: //write mode register
            channel = value & 3;
            dmachan[channel].direction = (value >> 5) & 1;
            dmachan[channel].autoinit = (value >> 4) & 1;
            dmachan[channel].writemode = (value >> 2) & 1; //not quite accurate
#ifdef DEBUG_DMA
            printf ("[NOTICE] DMA channel %u write mode reg: direction = %u, autoinit = %u, write mode = %u\n",
                    channel, dmachan[channel].direction, dmachan[channel].autoinit, dmachan[channel].writemode);
#endif
            break;
        case 0xC: //clear byte pointer flip-flop
#ifdef DEBUG_DMA
            printf ("[NOTICE] DMA cleared byte pointer flip-flop\n");
#endif
            flipflop = 0;
            break;
        case 0x83: //DMA channel 1 page register
            dmachan[1].page = (uint32_t) value << 16;
#ifdef DEBUG_DMA
            printf ("[NOTICE] DMA channel 1 page base = %05X\n", dmachan[1].page);
#endif
            break;
    }
}

__inline uint8_t in8237(uint16_t addr) {
#ifdef DEBUG_DMA
    printf ("in8237(0x%X);\n", addr);
#endif
    switch (addr) {
        case 3:
            if (flipflop == 1)
                return dmachan[1].reload >> 8;
            else
                return dmachan[1].reload;
            flipflop = ~flipflop & 1;    // this seems to be invalid, control never gets here ... :-O
            break;
        case 8:
            return 0x0F;
        break;
    }
    return 0xFF;
}

#endif
