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

#ifdef DMA_8237

static struct I8237_s {
	struct {
		uint32_t page;
		uint32_t addr;
		uint32_t reloadaddr;
		uint32_t addrinc;
		uint16_t count;
		uint16_t reloadcount;
		uint8_t autoinit;
		uint8_t mode;
		uint8_t enable;
		uint8_t masked;
		uint8_t dreq;
		uint8_t terminal;
		uint8_t operation;
	} channel[8];
	uint8_t flipflop;
	uint8_t tempreg;
	uint8_t memtomem;
} i8237;

#define DMA_MODE_DEMAND		0
#define DMA_MODE_SINGLE		1
#define DMA_MODE_BLOCK		2
#define DMA_MODE_CASCADE	3

#define DMA_OP_VERIFY		0
#define DMA_OP_WRITEMEM		1
#define DMA_OP_READMEM		2

void i8237_reset() {
	memset(&i8237, 0x00, sizeof(i8237));
	for (int i = 0; i < 8; ++i) {
	    i8237.channel[i].masked = 1;
		i8237.channel[i].addrinc = 1;
		i8237.channel[i].enable = 1;
	}
}

#ifdef DEBUG_DMA
char tmp[80];
#endif

void i8237_writeport(uint16_t addr16, uint8_t value) {
	uint8_t ch;
	uint8_t addr = addr16 & 0x0F;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] Write port %04Xh (%02Xh) v: %02Xh", addr16, addr, value); logMsg(tmp);
#endif
	switch (addr) {
	case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
		ch = (addr >> 1) & 3;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] Write port %04Xh (%02Xh) ch: %02Xh v: %02Xh", addr16, addr, ch, value); logMsg(tmp);
#endif
		if (addr & 0x01) { //write terminal count
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA]1 before: flipflop: %02Xh count: %04Xh reloadcount: %04Xh",
	                  i8237.flipflop, i8237.channel[ch].count, i8237.channel[ch].reloadcount); logMsg(tmp);
#endif
			if (i8237.flipflop) {
				i8237.channel[ch].count = (i8237.channel[ch].count & 0x00FF) | ((uint16_t)value << 8);
			}
			else {
				i8237.channel[ch].count = (i8237.channel[ch].count & 0xFF00) | (uint16_t)value;
			}
			i8237.channel[ch].reloadcount = i8237.channel[ch].count;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA]1 after: flipflop: %02Xh count: %04Xh reloadcount: %04Xh",
	                  i8237.flipflop, i8237.channel[ch].count, i8237.channel[ch].reloadcount); logMsg(tmp);
#endif
		}
		else {
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA]2 before: flipflop: %02Xh count: %04Xh reloadcount: %04Xh",
	                  i8237.flipflop, i8237.channel[ch].count, i8237.channel[ch].reloadcount); logMsg(tmp);
#endif
			if (i8237.flipflop) {
				i8237.channel[ch].addr = (i8237.channel[ch].addr & 0x00FF) | ((uint16_t)value << 8);
			}
			else {
				i8237.channel[ch].addr = (i8237.channel[ch].addr & 0xFF00) | (uint16_t)value;
			}
			i8237.channel[ch].reloadaddr = i8237.channel[ch].addr;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA]2 after: flipflop: %02Xh count: %04Xh reloadcount: %04Xh",
	                  i8237.flipflop, i8237.channel[ch].count, i8237.channel[ch].reloadcount); logMsg(tmp);
#endif
		}
		i8237.flipflop ^= 1;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] flipflop: %02Xh", i8237.flipflop); logMsg(tmp);
#endif
		break;
	case 0x08: //DMA channel 0-3 command register
		i8237.memtomem = value & 1;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] memtomem: %02Xh", i8237.memtomem); logMsg(tmp);
#endif
		break;
	case 0x09: //DMA request register
		i8237.channel[value & 3].dreq = (value >> 2) & 1;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] channel[value & 3(%d)].dreq: %02Xh", value & 3, i8237.channel[value & 3].dreq); logMsg(tmp);
#endif
		break;
	case 0x0A: //DMA channel 0-3 mask register
		i8237.channel[value & 3].masked = (value >> 2) & 1;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] channel[value & 3(%d)].masked: %02Xh", value & 3, i8237.channel[value & 3].masked); logMsg(tmp);
#endif
		break;
	case 0x0B: //DMA channel 0-3 mode register
		i8237.channel[value & 3].operation = (value >> 2) & 3;
		i8237.channel[value & 3].mode = (value >> 6) & 3;
		i8237.channel[value & 3].autoinit = (value >> 4) & 1;
		i8237.channel[value & 3].addrinc = (value & 0x20) ? 0xFFFFFFFF : 0x00000001;
		i8237.channel[value & 3].terminal = 0;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] channel[value & 3(%d)].operation: %02Xh ...", value & 3, i8237.channel[value & 3].operation); logMsg(tmp);
#endif
		break;
	case 0x0C: //clear byte pointer flip flop
		i8237.flipflop = 0;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] flipflop: %02Xh", i8237.flipflop); logMsg(tmp);
#endif
		break;
	case 0x0D: //DMA master clear
		i8237_reset(i8237);
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] i8237_reset"); logMsg(tmp);
#endif
		break;
	case 0x0F: //DMA write mask register
		i8237.channel[0].masked = value & 1;
		i8237.channel[1].masked = (value >> 1) & 1;
		i8237.channel[2].masked = (value >> 2) & 1;
		i8237.channel[3].masked = (value >> 3) & 1;
		i8237.channel[4].masked = (value >> 4) & 1;
		i8237.channel[5].masked = (value >> 5) & 1;
		i8237.channel[6].masked = (value >> 6) & 1;
		i8237.channel[7].masked = (value >> 7) & 1;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] mask register: %02Xh...", value); logMsg(tmp);
#endif
		break;
	}
}

void i8237_writepage(uint16_t addr16, uint8_t value) {
	uint8_t ch = addr16 & 0x000F;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] Write port %04Xh channel: %d : %02Xh", addr16, ch, value); logMsg(tmp);
#endif
    if (ch > 7) {
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] ERROR channel: %d", ch); logMsg(tmp);
#endif
		return;
	}
	i8237.channel[ch].page = (uint32_t)value << 16;
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] Channel %d page set to %08Xh", ch, i8237.channel[ch].page); logMsg(tmp);
#endif
}

uint8_t i8237_readport(uint16_t addr16) {
	uint8_t ch, ret = 0xFF;
	uint8_t addr = addr16 & 0x0F;
	switch (addr) {
	case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
		ch = (addr >> 1) & 3;
		if (addr & 1) { //count
			if (i8237.flipflop) {
				ret = (uint8_t)(i8237.channel[ch].count >> 8); //TODO: or give back the reload??
			}
			else {
				ret = (uint8_t)i8237.channel[ch].count; //TODO: or give back the reload??
			}
		} else { //address
			//printf("%04X\r\n", i8237.channel[ch].addr);
			if (i8237.flipflop) {
				ret = (uint8_t)(i8237.channel[ch].addr >> 8);
			}
			else {
				ret = (uint8_t)i8237.channel[ch].addr;
			}
		}
		i8237.flipflop ^= 1;
		break;
	case 0x08: //status register
		ret = 0x0F;
	}
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] Read port %04Xh channel %02Xh res %02Xh", addr16, addr, ret); logMsg(tmp);
#endif
	return ret;
}

uint8_t i8237_readpage(uint16_t addr) {
	uint8_t ch = addr & 0x000F;
	if (addr > 7) {
#ifdef DEBUG_DMA
        snprintf(tmp, 80, "[DMA] Read port %04Xh channel %01Xh err FFh", addr, ch); logMsg(tmp);
#endif
		return 0xFF;
	}
	uint8_t res = (uint8_t)(i8237.channel[ch].page >> 16);
#ifdef DEBUG_DMA
	snprintf(tmp, 80, "[DMA] Read port %04Xh channel %01Xh res %02Xh", addr, ch, res); logMsg(tmp);
#endif
	return res;
}

uint8_t i8237_read(uint8_t ch) {
	uint8_t ret = 0xFF;
	//TODO: fix commented out stuff
	if (i8237.channel[ch].enable && !i8237.channel[ch].terminal) {
		ret = read86(i8237.channel[ch].page + i8237.channel[ch].addr);
		i8237.channel[ch].addr += i8237.channel[ch].addrinc;
		i8237.channel[ch].count--;
		if (i8237.channel[ch].count == 0xFFFF) {
			if (i8237.channel[ch].autoinit) {
				i8237.channel[ch].count = i8237.channel[ch].reloadcount;
				i8237.channel[ch].addr = i8237.channel[ch].reloadaddr;
			} else {
				i8237.channel[ch].terminal = 1; //TODO: does this also happen in autoinit mode?
			}
		}
	}
	return ret;
}

void i8237_write(uint8_t ch, uint8_t value) {
	//TODO: fix commented out stuff
	if (i8237.channel[ch].enable && !i8237.channel[ch].terminal) {
	write86(i8237.channel[ch].page + i8237.channel[ch].addr, value);
#ifdef DEBUG_DMA
	snprintf(tmp, 80,"[DMA] RAM Write %02Xh to %05Xh count: %04Xh addrinc: %08Xh",
	              value, i8237.channel[ch].page + i8237.channel[ch].addr, i8237.channel[ch].count, i8237.channel[ch].addrinc
	); logMsg(tmp);
#endif
	i8237.channel[ch].addr += i8237.channel[ch].addrinc;
	i8237.channel[ch].count--;
	if (i8237.channel[ch].count == 0xFFFF) {
		if (i8237.channel[ch].autoinit) {
			i8237.channel[ch].count = i8237.channel[ch].reloadcount;
			i8237.channel[ch].addr = i8237.channel[ch].reloadaddr;
		}
		else {
			i8237.channel[ch].terminal = 1; //TODO: does this also happen in autoinit mode?
		}
	}
	}
}

#endif
