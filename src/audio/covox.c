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
static uint8_t samples_buffer[16] = { 0 };
static volatile uint8_t ssourceptr = 0, ssactive = 0;
uint8_t checks = 0;

int16_t tickssource() { // core #1
    if (ssourceptr == 0 || !ssactive || checks < 2) { // no bytes in buffer
        return 0;
    }

    uint16_t ssourcecursample = samples_buffer[0];

    for (int rotatefifo = 1; rotatefifo < 16; rotatefifo++) {
        samples_buffer[rotatefifo - 1] = samples_buffer[rotatefifo];
    }

    ssourceptr--;
    return ssourcecursample;
}

inline static void putssourcebyte(uint8_t value) { // core #0
    if (ssourceptr == 16)
        return;
    samples_buffer[ssourceptr++] = value;
    printf("SS BUFF %i\r\n", ssourceptr);
}

static inline uint8_t ssourcefull() {
    return ssourceptr == 16 ? 0x40 : 0x00;
}

void outsoundsource(uint16_t portnum, uint8_t value) {
    printf("OUT SS %x %x\r\n", portnum, value);
    static uint8_t last37a, port378 = 0;
    switch (portnum) {
        case 0x378:
            port378 = value;
            putssourcebyte(value);
            last37a = 0;
            break;
        case 0x37A:
           // Зачем слать предидущий байт в буфер если это не инит?
             if ((value & 4) && !(last37a & 4)) {
                 putssourcebyte(port378);
             }
            if (value == 0x04) {
                ssactive = 1;
            }
            last37a = value;
            break;
    }
}

uint8_t insoundsource(uint16_t portnum) {
    uint8_t v = ssourcefull();
    checks++;
    printf("IN SS %x %x checks %i\r\n", portnum,v, checks);
    return v;
}
