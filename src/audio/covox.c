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

/* ssource.c: functions to emulate the Disney Sound Source's 16-byte FIFO buffer. */
#include "../emulator.h"
// https://archive.org/details/dss-programmers-guide/page/n1/mode/2up
// https://groups.google.com/g/comp.sys.ibm.pc.games/c/gsz2CLJZsx4
#define COVOX_BUF_SZ 16
static uint8_t ssourcebuf[COVOX_BUF_SZ] = { 0 };
static volatile uint8_t ssourceptrIn = 0;
static volatile uint8_t ssourceptrOut = 0;
static volatile uint8_t free_buff_sz = COVOX_BUF_SZ;
static volatile uint8_t powerOn = 0;

int16_t tickssource() { // core #1
    if (free_buff_sz == 0 || !powerOn) { // no bytes in buffer or power is off
        return 0;
    }
    register int16_t res = ssourcebuf[ssourceptrOut++];
    if (ssourceptrOut >= COVOX_BUF_SZ) {
        ssourceptrOut = 0;
    }
    free_buff_sz++;
    return res;
}

inline static void putssourcebyte(uint8_t value) { // core #0
    if (free_buff_sz >= COVOX_BUF_SZ || !powerOn) { // ignore input, no free space in buffer or power is off
        return;
    }
    ssourcebuf[ssourceptrIn++] = value;
    if (ssourceptrIn >= COVOX_BUF_SZ) {
        ssourceptrIn = 0;
    }
    free_buff_sz--;
}

void outsoundsource(uint16_t portnum, uint8_t value) {
    static uint8_t prev = 4;
    switch (portnum) {
        case 0x378:
            putssourcebyte(value);
            break;
        case 0x37A:
            if (value == 4 && !powerOn) { // 4h - turn DSS on
                free_buff_sz = COVOX_BUF_SZ;
                ssourceptrIn, ssourceptrOut = 0;
                powerOn = 1;
            }
            else if ((value & 8) && powerOn) { // 0Eh / 0Ch - turn DSS off
                powerOn = 0;
            }
            break;
    }
}

uint8_t insoundsource(uint16_t portnum) {
    return (!powerOn || free_buff_sz == 0) ? 0x40 : 0x00;
}
