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
	} channel[4];
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
	//memset(&i8237, 0x00, sizeof(i8237) * 4);
	i8237.channel[0].masked = 1;
	i8237.channel[1].masked = 1;
	i8237.channel[2].masked = 1;
	i8237.channel[3].masked = 1;
}

void i8237_writeport(uint16_t addr, uint8_t value) {
	uint8_t ch;
#ifdef DEBUG_DMA
	printf("[DMA] Write port 0x%X: %X\n", addr, value);
#endif
	addr &= 0x0F;
	switch (addr) {
	case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7:
		ch = (addr >> 1) & 3;
		if (addr & 0x01) { //write terminal count
			if (i8237.flipflop) {
				i8237.channel[ch].count = (i8237.channel[ch].count & 0x00FF) | ((uint16_t)value << 8);
			}
			else {
				i8237.channel[ch].count = (i8237.channel[ch].count & 0xFF00) | (uint16_t)value;
			}
			i8237.channel[ch].reloadcount = i8237.channel[ch].count;
		}
		else {
			if (i8237.flipflop) {
				i8237.channel[ch].addr = (i8237.channel[ch].addr & 0x00FF) | ((uint16_t)value << 8);
			}
			else {
				i8237.channel[ch].addr = (i8237.channel[ch].addr & 0xFF00) | (uint16_t)value;
			}
			i8237.channel[ch].reloadaddr = i8237.channel[ch].addr;
#ifdef DEBUG_DMA
			printf("[DMA] Channel %u addr set to %08X\r\n", ch, i8237.channel[ch].addr);
#endif
		}
		i8237.flipflop ^= 1;
		break;
	case 0x08: //DMA channel 0-3 command register
		i8237.memtomem = value & 1;
		break;
	case 0x09: //DMA request register
		i8237.channel[value & 3].dreq = (value >> 2) & 1;
		break;
	case 0x0A: //DMA channel 0-3 mask register
		i8237.channel[value & 3].masked = (value >> 2) & 1;
		break;
	case 0x0B: //DMA channel 0-3 mode register
		i8237.channel[value & 3].operation = (value >> 2) & 3;
		i8237.channel[value & 3].mode = (value >> 6) & 3;
		i8237.channel[value & 3].autoinit = (value >> 4) & 1;
		i8237.channel[value & 3].addrinc = (value & 0x20) ? 0xFFFFFFFF : 0x00000001;
		break;
	case 0x0C: //clear byte pointer flip flop
		i8237.flipflop = 0;
		break;
	case 0x0D: //DMA master clear
		i8237_reset(i8237);
		break;
	case 0x0F: //DMA write mask register
		i8237.channel[0].masked = value & 1;
		i8237.channel[1].masked = (value >> 1) & 1;
		i8237.channel[2].masked = (value >> 2) & 1;
		i8237.channel[3].masked = (value >> 3) & 1;
		break;
	}
}

void i8237_writepage(uint16_t addr, uint8_t value) {
	uint8_t ch;
#ifdef DEBUG_DMA
	printf("[DMA] Write port 0x%X: %X\n", addr, value);
#endif
	addr &= 0x0F;
	switch (addr) {
	case 0x07:
		ch = 0;
		break;
	case 0x03:
		ch = 1;
		break;
	case 0x01:
		ch = 2;
		break;
	case 0x02:
		ch = 3;
		break;
	default:
		return;
	}
	i8237.channel[ch].page = (uint32_t)value << 16;
#ifdef DEBUG_DMA
	printf("[DMA] Channel %u page set to %08X\r\n", ch, i8237.channel[ch].page);
#endif
}

uint8_t i8237_readport(uint16_t addr) {
	uint8_t ch, ret = 0xFF;
#ifdef DEBUG_DMA
	printf("[DMA] Read port 0x%X\n", addr);
#endif

	addr &= 0x0F;
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
	return ret;
}

uint8_t i8237_readpage(uint16_t addr) {
	uint8_t ch;
#ifdef DEBUG_DMA
	printf("[DMA] Read port 0x%X\n", addr);
#endif
	addr &= 0x0F;
	switch (addr) {
	case 0x07:
		ch = 0;
		break;
	case 0x03:
		ch = 1;
		break;
	case 0x01:
		ch = 2;
		break;
	case 0x02:
		ch = 3;
		break;
	default:
		return 0xFF;
	}
	return (uint8_t)(i8237.channel[ch].page >> 16);
}

uint8_t i8237_read(uint8_t ch) {
	uint8_t ret = 0xFF;

	//TODO: fix commented out stuff
	//if (i8237.channel[ch].enable && !i8237.channel[ch].terminal) {
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
	//}

	return ret;
}

void i8237_write(uint8_t ch, uint8_t value) {
	//TODO: fix commented out stuff
	//if (i8237.channel[ch].enable && !i8237.channel[ch].terminal) {
	write86(i8237.channel[ch].page + i8237.channel[ch].addr, value);
#ifdef DEBUG_DMA
	printf("Write to %05X\r\n", i8237.channel[ch].page + i8237.channel[ch].addr);
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

#endif
