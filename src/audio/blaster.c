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

/* blaster.c: functions to emulate a Creative Labs Sound Blaster Pro. */

#include "../emulator.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
//#define DEBUG_BLASTER 1
#ifdef SOUND_BLASTER
#ifndef DMA8237
#define DMA8237
#endif
#endif

struct blaster_s {
	uint8_t mem[1024];
	uint16_t memptr;
	uint16_t samplerate;
	uint8_t dspmaj;
	uint8_t dspmin;
	uint8_t speakerstate;
	uint8_t lastresetval;
	uint8_t lastcmdval;
	uint8_t lasttestval;
	uint8_t waitforarg;
	uint8_t paused8;
	uint8_t paused16;
	uint8_t sample;
	uint8_t sbirq;
	uint8_t sbdma;
	uint8_t usingdma;
	uint8_t maskdma;
	uint8_t useautoinit;
	uint32_t blocksize;
	uint32_t blockstep;
	uint64_t sampleticks;
	struct mixer_s {
		uint8_t index;
		uint8_t reg[256];
	} mixer;
} blaster;


static void bufNewData ( uint8_t value )
{
	if (blaster.memptr >= sizeof(blaster.mem))
		return;
	blaster.mem[blaster.memptr] = value;
	blaster.memptr++;
}

static void setsampleticks ( void )
{
	if (blaster.samplerate == 0) {
		blaster.sampleticks = 0;
		return;
	}
	blaster.sampleticks = 44100 / (uint64_t)blaster.samplerate;
}


static void cmdBlaster ( uint8_t value )
{
	uint8_t recognized = 1;
	if (blaster.waitforarg) {
		switch (blaster.lastcmdval) {
			case 0x10: //direct 8-bit sample output
				blaster.sample = value;
				break;
			case 0x14: //8-bit single block DMA output
			case 0x24:
			case 0x91:
				if (blaster.waitforarg == 2) {
					blaster.blocksize = (blaster.blocksize & 0xFF00) | (uint32_t)value;
					blaster.waitforarg = 3;
					return;
				} else {
					blaster.blocksize = (blaster.blocksize & 0x00FF) | ( (uint32_t)value << 8);
#ifdef DEBUG_BLASTER
					printf("[NOTICE] Sound Blaster DSP block transfer size set to %u\n", blaster.blocksize);
#endif
					blaster.usingdma = 1;
					blaster.blockstep = 0;
					blaster.useautoinit = 0;
					blaster.paused8 = 0;
					blaster.speakerstate = 1;
				}
				break;
			case 0x40: //set time constant
				blaster.samplerate = (uint16_t)((uint32_t)1000000 / (uint32_t)(256 - (uint32_t)value));
				setsampleticks();
#ifdef DEBUG_BLASTER
				printf("[DEBUG] Sound Blaster time constant received, sample rate = %u\n", blaster.samplerate);
#endif
				break;
			case 0x48: //set DSP block transfer size
				if (blaster.waitforarg == 2) {
					blaster.blocksize = (blaster.blocksize & 0xFF00) | (uint32_t)value;
					blaster.waitforarg = 3;
					return;
				} else {
					blaster.blocksize = (blaster.blocksize & 0x00FF) | ((uint32_t)value << 8);
					//if (blaster.blocksize == 0)
					//	blaster.blocksize = 65536;
					blaster.blockstep = 0;
#ifdef DEBUG_BLASTER
					printf("[NOTICE] Sound Blaster DSP block transfer size set to %u\n", blaster.blocksize);
#endif
				}
				break;
			case 0xE0: //DSP identification for Sound Blaster 2.0 and newer (invert each bit and put in read buffer)
				bufNewData(~value);
				break;
			case 0xE4: //DSP write test, put data value into read buffer
				bufNewData(value);
				blaster.lasttestval = value;
				break;
			default:
				recognized = 0;
				break;
		}
		//blaster.waitforarg--; // = 0;
		if (recognized)
			return;
	}
	switch (value) {
			case 0x10:
			case 0x40:
			case 0xE0:
			case 0xE4:
				blaster.waitforarg = 1;
				break;

			case 0x14: //8-bit single block DMA output
			case 0x24:
			case 0x48:
			case 0x91:
				blaster.waitforarg = 2;
				break;

			case 0x1C: //8-bit auto-init DMA output
			case 0x2C:
				blaster.usingdma = 1;
				blaster.blockstep = 0;
				blaster.useautoinit = 1;
				blaster.paused8 = 0;
				blaster.speakerstate = 1;
				break;

			case 0xD0: //pause 8-bit DMA I/O
				blaster.paused8 = 1;
				break;	// FIXME: it was a missing break, I guess it was a mistake only!!
			case 0xD1: //speaker output on
				blaster.speakerstate = 1;
				break;
			case 0xD3: //speaker output off
				blaster.speakerstate = 0;
				break;
			case 0xD4: //continue 8-bit DMA I/O
				blaster.paused8 = 0;
				break;
			case 0xD8: //get speaker status
				if (blaster.speakerstate)
					bufNewData(0xFF);
				else
					bufNewData(0x00);
				break;
			case 0xDA: //exit 8-bit auto-init DMA I/O mode
				blaster.usingdma = 0;
				break;
			case 0xE1: //get DSP version info
				blaster.memptr = 0;
				bufNewData(blaster.dspmaj);
				bufNewData(blaster.dspmin);
				break;
			case 0xE8: //DSP read test
				blaster.memptr = 0;
				bufNewData(blaster.lasttestval);
				break;
			case 0xF2: //force 8-bit IRQ
				doirq(blaster.sbirq);
				break;
			case 0xF8: //undocumented command, clears in-buffer and inserts a null byte
				blaster.memptr = 0;
				bufNewData (0);
				break;
			default:
				printf("[NOTICE] Sound Blaster received unhandled command %02Xh\n", value);
				break;
		}
}


static uint8_t mixer[256], mixerindex = 0;


void outBlaster ( uint16_t portnum, uint8_t value )
{
#ifdef DEBUG_BLASTER
	printf("[DEBUG] outBlaster: port %Xh, value %02X\n", portnum, value);
#endif
	portnum &= 0xF;
	switch (portnum) {
		case 0x0:
		case 0x8:
			outadlib(0x388, value);
			break;
		case 0x1:
		case 0x9:
			outadlib(0x389, value);
			break;
		case 0x4: //mixer address port
			mixerindex = value;
			break;
		case 0x5: //mixer data
			mixer[mixerindex] = value;
			break;
		case 0x6: //reset port
			if ((value == 0x00) && (blaster.lastresetval == 0x01)) {
				blaster.speakerstate = 0;
				blaster.sample = 128;
				blaster.waitforarg = 0;
				blaster.memptr = 0;
				blaster.usingdma = 0;
				blaster.blocksize = 65535;
				blaster.blockstep = 0;
				bufNewData(0xAA);
				memset(mixer, 0xEE, sizeof(mixer));
#ifdef DEBUG_BLASTER
				printf("[DEBUG] Sound Blaster received reset!\n");
#endif
			}
			blaster.lastresetval = value;
			break;
		case 0xC: //write command/data
			cmdBlaster (value);
			if (blaster.waitforarg != 3)
				blaster.lastcmdval = value;
			break;
	}
}


uint8_t inBlaster ( uint16_t portnum )
{
	uint8_t ret = 0;
#ifdef DEBUG_BLASTER
	static uint16_t lastread = 0;
#endif
#ifdef DEBUG_BLASTER
	//if (lastread != portnum)
		printf("[DEBUG] inBlaster: port %Xh, value ", portnum);
#endif
	portnum &= 0xF;
	switch (portnum) {
		case 0x0:
		case 0x8:
			ret = inadlib (0x388);
			break;
		case 0x1:
		case 0x9:
			ret = inadlib (0x389);
			break;
		case 0x5: //mixer data
			ret = mixer[mixerindex];
			break;
		case 0xA: //read data
			if (blaster.memptr == 0) {
				ret = 0;
			} else {
				ret = blaster.mem[0];
				memmove(&blaster.mem[0], &blaster.mem[1], sizeof(blaster.mem) - 1);
				blaster.memptr--;
			}
			break;
		case 0xE: //read-buffer status
			if (blaster.memptr > 0)
				ret = 0x80;
			else
				ret = 0x00;
			break;
		default:
			ret = 0x00;
	}
#ifdef DEBUG_BLASTER
	//if (lastread != portnum)
	//	printf("%02X\n", ret);
	//lastread = portnum;
#endif
	return ret;
}


void tickBlaster( void )
{
	if (!blaster.usingdma)
		return;
	/*if (blaster.paused8) {
		blaster.sample = 128;
		return;
	  }*/

//	printf("tickBlaster();\n");
#ifdef DMA8237
	blaster.sample = read8237 (blaster.sbdma);
#endif
	blaster.blockstep++;
	if (blaster.blockstep > blaster.blocksize) {
		doirq (blaster.sbirq);
#ifdef DEBUG_BLASTER
		printf ("[NOTICE] Sound Blaster did IRQ\n");
#endif
		if (blaster.useautoinit) {
			blaster.blockstep = 0;
		} else {
			blaster.usingdma = 0;
		}
	}
}


int16_t getBlasterSample ( void )
{
	if (blaster.speakerstate == 0)
		return 0;
	else
		return (int16_t)blaster.sample;
}


static void mixerReset ( void )
{
	memset(blaster.mixer.reg, 0, sizeof (blaster.mixer.reg) );
	blaster.mixer.reg[0x22] = blaster.mixer.reg[0x26] = blaster.mixer.reg[0x04] = (4 << 5) | (4 << 1);
}


void initBlaster ( uint16_t baseport, uint8_t irq )
{
	memset(&blaster, 0, sizeof (blaster) );
	blaster.dspmaj = 2; //emulate a Sound Blaster 2.0
	blaster.dspmin = 0;
	blaster.sbirq = 7;
	blaster.sbdma = 1;
	mixerReset();
	printf("SB INIT\r\n");
//	set_port_write_redirector(baseport, baseport + 0xE, &outBlaster);
	// set_port_read_redirector(baseport, baseport + 0xE, &inBlaster);
}
