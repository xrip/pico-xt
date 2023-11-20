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

/* sermouse.c: functions to emulate a standard Microsoft-compatible serial mouse. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <vga.h>

#include "emulator.h"

struct sermouse_s {
	uint8_t	reg[8];
	uint8_t	buf[16];
	int8_t	bufptr;
} sermouse;


__inline static void bufsermousedata ( uint8_t value )
{
	if (sermouse.bufptr == 16)
		return;
	if (sermouse.bufptr == 0) {
		doirq(4);
	}
	sermouse.buf[sermouse.bufptr++] = value;
}


__inline  void outsermouse ( uint16_t portnum, uint8_t value )
{
	uint8_t oldreg;

	portnum &= 7;
	/*char tmp[80];
	sprintf(tmp, "[DEBUG] Serial mouse, port %X out: %02X\n", portnum, value);
	logMsg(tmp);*/
	oldreg = sermouse.reg[portnum];
	sermouse.reg[portnum] = value;
	switch (portnum) {
		case 4: //modem control register
			if ((value & 1) != (oldreg & 1)) {	//software toggling of this register
				sermouse.bufptr = 0;	//causes the mouse to reset and fill the buffer
				bufsermousedata('M');	//with a bunch of ASCII 'M' characters.
				bufsermousedata('M');	//this is intended to be a way for
				bufsermousedata('M');	//drivers to verify that there is
				bufsermousedata('M');	//actually a mouse connected to the port.
				bufsermousedata('M');
				bufsermousedata('M');
			}
			break;
	}
}


__inline uint8_t insermouse ( uint16_t portnum )
{
	uint8_t temp;
	portnum &= 7;
	/*char tmp[80];
	sprintf(tmp, "[DEBUG] Serial mouse, port %X in\n", portnum);
	logMsg(tmp);*/
	switch (portnum) {
		case 0:	//data receive
			temp = sermouse.buf[0];
			memmove (sermouse.buf, &sermouse.buf[1], 15);
			sermouse.bufptr--;
			if (sermouse.bufptr < 0)
				sermouse.bufptr = 0;
			if (sermouse.bufptr > 0)
				doirq(4);
			sermouse.reg[4] = ~sermouse.reg[4] & 1;
			return temp;
		case 5:	//line status register (read-only)
			if (sermouse.bufptr > 0)
				temp = 1;
			else
				temp = 0;
			return 0x1;
			return 0x60 | temp;
	}
	return sermouse.reg[portnum & 7];
}


void initsermouse ( uint16_t baseport, uint8_t irq )
{
	sermouse.bufptr = 0;
}


void sermouseevent ( uint8_t buttons, int8_t xrel, int8_t yrel )
{
	uint8_t highbits = (xrel < 0) ? 3 : 0;
	if (yrel < 0)
		highbits |= 12;
	bufsermousedata(0x40 | (buttons << 4) | highbits);
	bufsermousedata(xrel & 63);
	bufsermousedata(yrel & 63);
}
