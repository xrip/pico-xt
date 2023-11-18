/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2012 Mike Chambers
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

#ifndef FAKE86_I8237_H_INCLUDED
#define FAKE86_I8237_H_INCLUDED

#include <stdint.h>

struct dmachan_s {
	uint32_t page;
	uint32_t addr;
	uint32_t reload;
	uint32_t count;
	uint8_t direction;
	uint8_t autoinit;
	uint8_t writemode;
	uint8_t masked;
};

extern void init8237(void);
extern uint8_t read8237 (uint8_t channel);

#endif
