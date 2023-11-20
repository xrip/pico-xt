/*
  Fake86: A portable, open-source 8086 PC emulator.
  Copyright (C)2010-2013 Mike Chambers

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

#include "emulator.h"

#if PICO_ON_DEVICE

#include "pico/time.h"
#include "ps2.h"
#include "vga.h"
#include "psram_spi.h"
extern psram_spi_inst_t psram_spi;

#else

#include "SDL2/SDL.h"

#endif

uint8_t opcode, segoverride, reptype, hdcount = 0, fdcount = 0, hltstate = 0;
uint16_t segregs[4], ip, useseg, oldsp;
uint8_t tempcf, oldcf, cf, pf, af, zf, sf, tf, ifl, df, of, mode, reg, rm;
uint8_t videomode = 3;
int timer_period = 54925;

uint8_t byteregtable[8] = { regal, regcl, regdl, regbl, regah, regch, regdh, regbh };

static const uint8_t parity[0x100] = {
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
    0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1
};
__aligned(4096) uint8_t RAM[RAM_SIZE << 10];
uint8_t VRAM[VRAM_SIZE << 10];
#if PSEUDO_RAM_BASE || CD_CARD_SWAP
uint16_t PSEUDO_RAM_PAGES[PSEUDO_RAM_BLOCKS] = { 0 }; // 4KB blocks
uint16_t RAM_PAGES[RAM_BLOCKS] = { 0 };
#endif

uint8_t oper1b, oper2b, res8, nestlev, addrbyte;
uint16_t saveip, savecs, oper1, oper2, res16, disp16, temp16, dummy, stacksize, frametemp;
uint32_t temp1, temp2, temp3, ea;
uint64_t totalexec;

union _bytewordregs_ regs;

void modregrm() {
    addrbyte = getmem8(CPU_CS, ip);
    StepIP(1);
    mode = addrbyte >> 6;
    reg = (addrbyte >> 3) & 7;
    rm = addrbyte & 7;
    switch (mode) {
        case 0:
            if (rm == 6) {
                disp16 = getmem16(CPU_CS, ip);
                StepIP(2);
            }
            if (((rm == 2) || (rm == 3)) && !segoverride) {
                useseg = CPU_SS;
            }
            break;
        case 1:
            disp16 = signext(getmem8(CPU_CS, ip));
            StepIP(1);
            if (((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) {
                useseg = CPU_SS;
            }
            break;
        case 2:
            disp16 = getmem16(CPU_CS, ip);
            StepIP(2);
            if (((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) {
                useseg = CPU_SS;
            }
            break;
        default:
            disp16 = 0;
    }
}

#if PSEUDO_RAM_BASE || CD_CARD_SWAP
static uint16_t last_ram_page = 1;
uint32_t get_ram_page_for(const uint32_t addr32) {
    uint32_t flash_page = addr32 / RAM_PAGE_SIZE; // 4KB page idx
    uint16_t flash_page_desc = PSEUDO_RAM_PAGES[flash_page];
    if (flash_page_desc & 0x8000) { // higest (15) bit is set, it means - the page already in RAM
         return flash_page_desc & 0x7FFF; // actually max 256 4k pages (1MB), but may be more;)
    }
    // char tmp[40]; sprintf(tmp, "VRAM page: 0x%X", flash_page); logMsg(tmp);
    // rolling page usage
    uint16_t ram_page = last_ram_page++;
    if (last_ram_page >= RAM_BLOCKS - 1) last_ram_page = 1; // do not use first page
    uint16_t ram_page_desc = RAM_PAGES[ram_page];
    bool ro_page_was_found = !(ram_page_desc & 0x8000); // higest (15) bit is set, it means - the page has changes (RW page)
    uint16_t old_flash_page = ram_page_desc & 0x7FFF; // 14-0 - max 32k keys for PSEUDO_RAM_PAGES array
    if (old_flash_page > 0) {
        PSEUDO_RAM_PAGES[old_flash_page] = 0x0000; // not more mapped
    }
    PSEUDO_RAM_PAGES[flash_page] = 0x8000 | ram_page; // in ram + ram_page
    RAM_PAGES[ram_page] = flash_page;
    if (ro_page_was_found) { // just replace RO page (faster than RW flush to flash)
        // sprintf(tmp, "1 RAM page 0x%X / VRAM page: 0x%X", ram_page, flash_page); logMsg(tmp);
        uint32_t ram_page_offset = ram_page * RAM_PAGE_SIZE;
        uint32_t flash_page_offset = flash_page * RAM_PAGE_SIZE;
#if CD_CARD_SWAP
        read_vram_block(RAM + ram_page_offset, flash_page_offset, RAM_PAGE_SIZE);
#else
        memcpy(RAM + ram_page_offset, (const char*)PSEUDO_RAM_BASE + flash_page_offset, RAM_PAGE_SIZE);
#endif
        return ram_page;
    }
    // Lets flush found RW page to flash
    // sprintf(tmp, "2 RAM page 0x%X / VRAM page: 0x%X", ram_page, flash_page); logMsg(tmp);
    uint32_t ram_page_offset = ram_page * RAM_PAGE_SIZE;
    uint32_t flash_page_offset = old_flash_page * RAM_PAGE_SIZE;
    // sprintf(tmp, "2 RAM offs 0x%X / VRAM offs: 0x%X", ram_page_offset, flash_page_offset); logMsg(tmp);
#if CD_CARD_SWAP
    flush_vram_block(RAM + ram_page_offset, flash_page_offset, RAM_PAGE_SIZE);
#else
    flash_range_program3(
        PSEUDO_RAM_BASE + flash_page_offset,
        (const u_int8_t*)RAM + ram_page_offset,
        RAM_PAGE_SIZE
    );
#endif
    // use new page:
    flash_page_offset = flash_page * RAM_PAGE_SIZE;
#if CD_CARD_SWAP
    read_vram_block(RAM + ram_page_offset, flash_page_offset, RAM_PAGE_SIZE);
#else
    memcpy(RAM + ram_page_offset, (const char*)PSEUDO_RAM_BASE + flash_page_offset, RAM_PAGE_SIZE);
#endif
    return ram_page;
}
#endif

__inline void write86(uint32_t addr32, uint8_t value) {
#if PSEUDO_RAM_BASE || CD_CARD_SWAP
    if (!PSRAM_AVAILABLE && addr32 < RAM_PAGE_SIZE) { // do not touch first page
        RAM[addr32] = value;
    } else if (!PSRAM_AVAILABLE && addr32 < (PSEUDO_RAM_SIZE << 10)) {
        uint32_t ram_page = get_ram_page_for(addr32);
        uint32_t addr_in_page = addr32 & RAM_IN_PAGE_ADDR_MASK;
        RAM[(ram_page * RAM_PAGE_SIZE) + addr_in_page] = value;
        uint16_t ram_page_desc = RAM_PAGES[ram_page];
        if (!(ram_page_desc & 0x8000)) { // if higest (15) bit is set, it means - the page has changes
            RAM_PAGES[ram_page] = ram_page_desc | 0x8000; // mark it as changed - bit 15
        }
#endif
        return;
    } else if (((addr32) >= 0xB8000UL) && ((addr32) < 0xC0000UL)) {
        // video RAM range
        addr32 -= 0xB8000UL;
        VRAM[addr32] = value; // 16k for graphic mode!!!
        return;
    } else if (((addr32) >= 0xD0000UL) && ((addr32) < 0xD8000UL)) {
        addr32 -= 0xCC000UL;
    }
    else if ((addr32) > 0xFFFFFUL) {
        //SRAM_write(addr32, value);
    }
#if PICO_ON_DEVICE
    if (PSRAM_AVAILABLE) {
        psram_write8(&psram_spi, addr32, value);
    }
#endif
}

static __inline void writew86(uint32_t addr32, uint16_t value) {
#if PICO_ON_DEVICE
    if (PSRAM_AVAILABLE && (addr32 > (RAM_SIZE << 10) && addr32 < (640 << 10))) {
        psram_write16(&psram_spi, addr32, value);
    }
    else
#endif
    {
        write86(addr32, (uint8_t) value);
        write86(addr32 + 1, (uint8_t) (value >> 8));
    }
}

__inline uint8_t read86(uint32_t addr32) {
    //printf("read86 %lx\r\n", addr32);
#if PSEUDO_RAM_BASE || CD_CARD_SWAP
    if ((!PSRAM_AVAILABLE && addr32 < (PSEUDO_RAM_SIZE << 10)) || PSRAM_AVAILABLE && addr32 < (RAM_SIZE << 10)) {
#else
    if (addr32 < (RAM_SIZE << 10)) {
#endif
        // https://docs.huihoo.com/gnu_linux/own_os/appendix-bios_memory_2.htm
        switch (addr32) { //some hardcoded values for the BIOS data area
            /*case 0x400:     // serial COM1: address at 0x03F8
                return (0xF8);
            case 0x401:
                return (0x03);*/

            case 0x408: /// LPT1
                return (0x78);
            case 0x409:
                return (0x3);
            case 0x410:
                return (0b01100001); // video type CGA 80x25
            /*            76543210  40:10 (value in INT 11 register AL)
                          |||||||`- IPL diskette installed
                          ||||||`-- math coprocessor
                          |||||+--- old PC system board RAM < 256K
                          |||||`--- pointing device installed (PS/2)
                          ||||`---- not used on PS/2
                          ||`------ initial video mode
                          `-------- number of diskette drives, less 1
            */
            case 0x411:
                return (0b01000010);
/*  	                  76543210  40:11  (value in INT 11 register AH)
		                  |||||||`- 0 if DMA installed
		                  ||||  `-- number of serial ports
		                  |||`----- game adapter
		                  ||`------ not used, internal modem (PS/2)
		                  `-------- number of printer ports
 */

                /*
            case 0x413:
                return (RAM_SIZE & 0xFF);
            case 0x414:
                return ((RAM_SIZE >> 8) & 0xFF);
            */
            case 0x463:
                return (0xd4);
            case 0x464:
                return (0x3);
/*
            case 0x465:
                switch (videomode) {
                    case 0:
                        return (0x2C);
                    case 1:
                        return (0x28);
                    case 2:
                        return (0x2D);
                    case 3:
                        return (0x29);
                    case 4:
                        return (0x0E);
                    case 5:
                        return (0x0A);
                    case 6:
                        return (0x1E);
                    default:
                        return (0x29);
                }
*/
            case 0x466:
                return port3D9;
            case 0x475: //hard drive count
                return (hdcount);
            default:
#if PSEUDO_RAM_BASE || CD_CARD_SWAP
            if (PSRAM_AVAILABLE || addr32 < 4096) { // do not touch first 4kb
                return RAM[addr32];
            } else {
                uint32_t ram_page = get_ram_page_for(addr32);
                uint32_t addr_in_page = addr32 & RAM_IN_PAGE_ADDR_MASK;
                return RAM[(ram_page * RAM_PAGE_SIZE) + addr_in_page];
            }
#else
                return RAM[addr32];
#endif
        }
    }
    else if ((addr32 >= 0xFE000UL) && (addr32 <= 0xFFFFFUL)) {
        // BIOS ROM range
        addr32 -= 0xFE000UL;
        return BIOS[addr32];
    }
    else if ((addr32 >= 0xB8000UL) && (addr32 < 0xC0000UL)) {
        // video RAM range
        addr32 -= 0xB8000UL;
        return VRAM[addr32]; //
    }
    else if ((addr32 >= 0xD0000UL) && (addr32 < 0xD8000UL)) { // NE2000
        addr32 -= 0xCC000UL;
    }
    else if ((addr32 >= 0xF6000UL) && (addr32 < 0xFA000UL)) {
        // IBM BASIC ROM LOW
        addr32 -= 0xF6000UL;
        return BASICL[addr32];
    }
    else if ((addr32 >= 0xFA000UL) && (addr32 < 0xFE000UL)) {
        // IBM BASIC ROM HIGH
        addr32 -= 0xFA000UL;
        return BASICH[addr32];
    }
    else if ((addr32) > 0xFFFFFUL) {
        //SRAM_read(addr32);
    }
#if PICO_ON_DEVICE
    if (PSRAM_AVAILABLE) {
        return psram_read8(&psram_spi, addr32);
    }
#endif
    return 0;
}

static __inline uint16_t readw86(uint32_t addr32) {
#if PICO_ON_DEVICE
    if (PSRAM_AVAILABLE && (addr32 > (RAM_SIZE << 10) && addr32 < (640 << 10))) {
        // TODO: 16-bit read from vram page
        return psram_read16(&psram_spi, addr32);
    }
#endif
    return ((uint16_t)read86(addr32) | (uint16_t)(read86(addr32 + 1) << 8));
}

void flag_szp8(uint8_t value) {
    if (!value) {
        zf = 1;
    }
    else {
        zf = 0; /* set or clear zero flag */
    }

    if (value & 0x80) {
        sf = 1;
    }
    else {
        sf = 0; /* set or clear sign flag */
    }

    pf = parity[value]; /* retrieve parity state from lookup table */
}

void flag_szp16(uint16_t value) {
    if (!value) {
        zf = 1;
    }
    else {
        zf = 0; /* set or clear zero flag */
    }

    if (value & 0x8000) {
        sf = 1;
    }
    else {
        sf = 0; /* set or clear sign flag */
    }

    pf = parity[value & 255]; /* retrieve parity state from lookup table */
}

void flag_log8(uint8_t value) {
    flag_szp8(value);
    cf = 0;
    of = 0; /* bitwise logic ops always clear carry and overflow */
}

void flag_log16(uint16_t value) {
    flag_szp16(value);
    cf = 0;
    of = 0; /* bitwise logic ops always clear carry and overflow */
}

void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3) {
    /* v1 = destination operand, v2 = source operand, v3 = carry flag */
    uint16_t dst;

    dst = (uint16_t)v1 + (uint16_t)v2 + (uint16_t)v3;
    flag_szp8((uint8_t)dst);
    if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
        of = 1;
    }
    else {
        of = 0; /* set or clear overflow flag */
    }

    if (dst & 0xFF00) {
        cf = 1;
    }
    else {
        cf = 0; /* set or clear carry flag */
    }

    if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
        af = 1;
    }
    else {
        af = 0; /* set or clear auxilliary flag */
    }
}

void flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3) {
    uint32_t dst;

    dst = (uint32_t)v1 + (uint32_t)v2 + (uint32_t)v3;
    flag_szp16((uint16_t)dst);
    if ((((dst ^ v1) & (dst ^ v2)) & 0x8000) == 0x8000) {
        of = 1;
    }
    else {
        of = 0;
    }

    if (dst & 0xFFFF0000) {
        cf = 1;
    }
    else {
        cf = 0;
    }

    if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
        af = 1;
    }
    else {
        af = 0;
    }
}

void flag_add8(uint8_t v1, uint8_t v2) {
    /* v1 = destination operand, v2 = source operand */
    uint16_t dst;

    dst = (uint16_t)v1 + (uint16_t)v2;
    flag_szp8((uint8_t)dst);
    if (dst & 0xFF00) {
        cf = 1;
    }
    else {
        cf = 0;
    }

    if (((dst ^ v1) & (dst ^ v2) & 0x80) == 0x80) {
        of = 1;
    }
    else {
        of = 0;
    }

    if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
        af = 1;
    }
    else {
        af = 0;
    }
}

void flag_add16(uint16_t v1, uint16_t v2) {
    /* v1 = destination operand, v2 = source operand */
    uint32_t dst;

    dst = (uint32_t)v1 + (uint32_t)v2;
    flag_szp16((uint16_t)dst);
    if (dst & 0xFFFF0000) {
        cf = 1;
    }
    else {
        cf = 0;
    }

    if (((dst ^ v1) & (dst ^ v2) & 0x8000) == 0x8000) {
        of = 1;
    }
    else {
        of = 0;
    }

    if (((v1 ^ v2 ^ dst) & 0x10) == 0x10) {
        af = 1;
    }
    else {
        af = 0;
    }
}

void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3) {
    /* v1 = destination operand, v2 = source operand, v3 = carry flag */
    uint16_t dst;

    v2 += v3;
    dst = (uint16_t)v1 - (uint16_t)v2;
    flag_szp8((uint8_t)dst);
    if (dst & 0xFF00) {
        cf = 1;
    }
    else {
        cf = 0;
    }

    if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
        of = 1;
    }
    else {
        of = 0;
    }

    if ((v1 ^ v2 ^ dst) & 0x10) {
        af = 1;
    }
    else {
        af = 0;
    }
}

void flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3) {
    /* v1 = destination operand, v2 = source operand, v3 = carry flag */
    uint32_t dst;

    v2 += v3;
    dst = (uint32_t)v1 - (uint32_t)v2;
    flag_szp16((uint16_t)dst);
    if (dst & 0xFFFF0000) {
        cf = 1;
    }
    else {
        cf = 0;
    }

    if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
        of = 1;
    }
    else {
        of = 0;
    }

    if ((v1 ^ v2 ^ dst) & 0x10) {
        af = 1;
    }
    else {
        af = 0;
    }
}

void flag_sub8(uint8_t v1, uint8_t v2) {
    /* v1 = destination operand, v2 = source operand */
    uint16_t dst;

    dst = (uint16_t)v1 - (uint16_t)v2;
    flag_szp8((uint8_t)dst);
    if (dst & 0xFF00) {
        cf = 1;
    }
    else {
        cf = 0;
    }

    if ((dst ^ v1) & (v1 ^ v2) & 0x80) {
        of = 1;
    }
    else {
        of = 0;
    }

    if ((v1 ^ v2 ^ dst) & 0x10) {
        af = 1;
    }
    else {
        af = 0;
    }
}

void flag_sub16(uint16_t v1, uint16_t v2) {
    /* v1 = destination operand, v2 = source operand */
    uint32_t dst;

    dst = (uint32_t)v1 - (uint32_t)v2;
    flag_szp16((uint16_t)dst);
    if (dst & 0xFFFF0000) {
        cf = 1;
    }
    else {
        cf = 0;
    }

    if ((dst ^ v1) & (v1 ^ v2) & 0x8000) {
        of = 1;
    }
    else {
        of = 0;
    }

    if ((v1 ^ v2 ^ dst) & 0x10) {
        af = 1;
    }
    else {
        af = 0;
    }
}

static inline void op_adc8() {
    res8 = oper1b + oper2b + cf;
    flag_adc8(oper1b, oper2b, cf);
}

static inline void op_adc16() {
    res16 = oper1 + oper2 + cf;
    flag_adc16(oper1, oper2, cf);
}

static inline void op_add8() {
    res8 = oper1b + oper2b;
    flag_add8(oper1b, oper2b);
}

static inline void op_add16() {
    res16 = oper1 + oper2;
    flag_add16(oper1, oper2);
}

static inline void op_and8() {
    res8 = oper1b & oper2b;
    flag_log8(res8);
}

static inline void op_and16() {
    res16 = oper1 & oper2;
    flag_log16(res16);
}

static inline void op_or8() {
    res8 = oper1b | oper2b;
    flag_log8(res8);
}

static inline void op_or16() {
    res16 = oper1 | oper2;
    flag_log16(res16);
}

static inline void op_xor8() {
    res8 = oper1b ^ oper2b;
    flag_log8(res8);
}

static inline void op_xor16() {
    res16 = oper1 ^ oper2;
    flag_log16(res16);
}

static inline void op_sub8() {
    res8 = oper1b - oper2b;
    flag_sub8(oper1b, oper2b);
}

static inline void op_sub16() {
    res16 = oper1 - oper2;
    flag_sub16(oper1, oper2);
}

static inline void op_sbb8() {
    res8 = oper1b - (oper2b + cf);
    flag_sbb8(oper1b, oper2b, cf);
}

static inline void op_sbb16() {
    res16 = oper1 - (oper2 + cf);
    flag_sbb16(oper1, oper2, cf);
}

static inline void getea(uint8_t rmval) {
    uint32_t tempea;

    tempea = 0;
    switch (mode) {
        case 0:
            switch (rmval) {
                case 0:
                    tempea = CPU_BX + CPU_SI;
                    break;
                case 1:
                    tempea = CPU_BX + CPU_DI;
                    break;
                case 2:
                    tempea = CPU_BP + CPU_SI;
                    break;
                case 3:
                    tempea = CPU_BP + CPU_DI;
                    break;
                case 4:
                    tempea = CPU_SI;
                    break;
                case 5:
                    tempea = CPU_DI;
                    break;
                case 6:
                    tempea = disp16;
                    break;
                case 7:
                    tempea = CPU_BX;
                    break;
            }
            break;

        case 1:
        case 2:
            switch (rmval) {
                case 0:
                    tempea = CPU_BX + CPU_SI + disp16;
                    break;
                case 1:
                    tempea = CPU_BX + CPU_DI + disp16;
                    break;
                case 2:
                    tempea = CPU_BP + CPU_SI + disp16;
                    break;
                case 3:
                    tempea = CPU_BP + CPU_DI + disp16;
                    break;
                case 4:
                    tempea = CPU_SI + disp16;
                    break;
                case 5:
                    tempea = CPU_DI + disp16;
                    break;
                case 6:
                    tempea = CPU_BP + disp16;
                    break;
                case 7:
                    tempea = CPU_BX + disp16;
                    break;
            }
            break;
    }

    ea = (tempea & 0xFFFF) + (useseg << 4);
}

static inline void push(uint16_t pushval) {
    CPU_SP = CPU_SP - 2;
    putmem16(CPU_SS, CPU_SP, pushval);
}

static inline uint16_t pop() {
    uint16_t tempval;

    tempval = getmem16(CPU_SS, CPU_SP);
    CPU_SP = CPU_SP + 2;
    return tempval;
}

#if !PICO_ON_DEVICE
uint32_t ClockTick(uint32_t interval, void *name) {
    doirq(0);
    tickssource();
    return timer_period / 1000;
}

uint32_t BlinkTimer(uint32_t interval, void *name) {
    cursor_blink_state ^= 1;
    return interval;
}
#endif

#if PSEUDO_RAM_BASE
#include <hardware/flash.h>
#endif

void reset86() {
#if !PICO_ON_DEVICE
    SDL_AddTimer(timer_period / 1000, ClockTick, "timer");
    SDL_AddTimer(500, BlinkTimer, "blink");
#endif
    init8253();
    init8259();
    memset(RAM, 0x0, RAM_SIZE << 10);
    memset(VRAM, 0x0, VRAM_SIZE << 10);
#if PSEUDO_RAM_BASE || CD_CARD_SWAP
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    for (size_t page = 0; page < PSEUDO_RAM_BLOCKS; ++page) {
        if (page < RAM_BLOCKS) {
            PSEUDO_RAM_PAGES[page] = 0x8000 | page;
            RAM_PAGES[page] = page;
        } else {
            PSEUDO_RAM_PAGES[page] = 0;
        }
    }
    /* Ctrl+Alt+Del issue TODO: detect why
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(PSEUDO_RAM_BASE - XIP_BASE, PSEUDO_RAM_SIZE);
    restore_interrupts(interrupts);
    gpio_put(PICO_DEFAULT_LED_PIN, false);
    */
#endif

    CPU_CS = 0xFFFF;
    CPU_SS = 0x0000;
    CPU_SP = 0x0000;

    ip = 0x0000;

    hltstate = 0;
    videomode = 3;
}

uint16_t readrm16(uint8_t rmval) {
    if (mode < 3) {
        getea(rmval);
        return read86(ea) | ((uint16_t)read86(ea + 1) << 8);
    }
    else {
        return getreg16(rmval);
    }
}

uint8_t readrm8(uint8_t rmval) {
    if (mode < 3) {
        getea(rmval);
        return read86(ea);
    }
    else {
        return getreg8(rmval);
    }
}

void writerm16(uint8_t rmval, uint16_t value) {
    if (mode < 3) {
        getea(rmval);
        write86(ea, value & 0xFF);
        write86(ea + 1, value >> 8);
    }
    else {
        putreg16(rmval, value);
    }
}

void writerm8(uint8_t rmval, uint8_t value) {
    if (mode < 3) {
        getea(rmval);
        write86(ea, value);
    }
    else {
        putreg8(rmval, value);
    }
}

uint8_t tandy_hack = 0;

void intcall86(uint8_t intnum) {
    switch (intnum) {
        case 0x10:
            //printf("INT 10h CPU_AH: 0x%x CPU_AL: 0x%x\r\n", CPU_AH, CPU_AL);
            switch (CPU_AH) {
                case 0x00:
                    videomode = CPU_AL & 0x7F;
                    if (videomode == 4) {
                        port3D9 = 48;
                    }
                    else {
                        port3D9 = 0;
                    }

                // FIXME!!
                    RAM[0x449] = videomode;
                    RAM[0x44A] = (uint8_t)videomode <= 2 ? 40 : 80;
                    RAM[0x44B] = 0;
                    RAM[0x484] = (uint8_t)(25 - 1);
#ifdef EGA
                    if ((CPU_AL & 0x80) == 0x00) {
                        memset(VRAM, 0x0, sizeof VRAM);
                    }
#endif
                // http://www.techhelpmanual.com/114-video_modes.html
                // http://www.techhelpmanual.com/89-video_memory_layouts.html
                    printf("VBIOS: Mode 0x%x (0x%x)\r\n", CPU_AX, videomode);
#if PICO_ON_DEVICE
                    switch (videomode) {
                        case 0:
                        case 1:
                            graphics_set_mode(TEXTMODE_40x30);
                            break;
                        case 2:
                        case 3:
                            graphics_set_mode(TEXTMODE_80x30);
                            break;
                        case 4:
                        case 5:
                            if (!tandy_hack) {
                                graphics_set_mode(CGA_320x200x4);
                            }
                            else {
                                tandy_hack = 0;
                            }
                            break;
                        case 6:
                            graphics_set_mode(CGA_640x200x2);
                            break;
                        case 8:
                        case 0x15:
                            for (int i = 0; i < 16; i++) {
                                graphics_set_palette(i, tandy_palette[i]);
                            }
                            graphics_set_mode(CGA_160x200x16);
                            tandy_hack = 1;
                            break;
                        case 0x09:
                            for (int i = 0; i < 16; i++) {
                                graphics_set_palette(i, tandy_palette[i]);
                            }
                            graphics_set_mode(TGA_320x200x16);
                            break;
                    }
#endif
                // Установить видеорежим
                    break;
                /*
                                case 0x1A: //get display combination code (ps, vga/mcga)
                                    CPU_AL = 0x1A;
                                    CPU_BL = 0x8;
                                    break;

                                case 0x01:                            // show cursor
                                    if (regs.byteregs[regch] == 0x32) {
                                        curshow = false;
                                        break;
                                    }
                                    else if ((regs.byteregs[regch] == 0x06) && (regs.byteregs[regcl] == 0x07)) {
                                        CURSOR_ASCII = 95;
                                        curshow = true;
                                        break;
                                    }
                                    else if ((regs.byteregs[regch] == 0x00) && (regs.byteregs[regcl] == 0x07)) {
                                        CURSOR_ASCII = 219;
                                        curshow = true;
                                    }
                                    break;
                */
#if 0
                    case 0x1A: //get display combination code (ps, vga/mcga)
                        CPU_AL = 0x1A;
                        CPU_BL = 0x04;
                        CPU_BH = 0x08;
                        return;

                    case 0x02: // Установить позицию курсора
                        CURX = CPU_DL;
                        CURY = CPU_DH;
                        return;
                    case 0x03: // Получить позицию курсора
                        CPU_DL = CURX;
                        CPU_DH = CURY;
                        return;
                    case 0x06:
                        if (!CPU_AL) {
                            // FIXME!! Нормально сделай!
                            memset(VRAM, 0x00, 160 * 25);
                            return;
                        }
                        break;
                    case 0x08: // Получим чар под курсором
                        CPU_AL = VRAM[(CURY * 160 + CURX * 2) + 0];
                        CPU_AH = VRAM[(CURY * 160 + CURX * 2) + 1];
                        return;
                    case 0x09:
                        /*09H писать символ/атрибут в текущей позиции курсора
                           вход:  BH = номер видео страницы
                           AL = записываемый символ
                           CX = счетчик (сколько экземпляров символа записать)
                           BL = видео атрибут (текст) или цвет (графика)
                            (графические режимы: +80H означает XOR с символом на экране)*/
                        //printf("color %c %x %i\r\n",  CPU_AL, CPU_CX, CPU_BL);
                        color = CPU_BL;
                    case 0x0A:
                        /*0aH писать символ в текущей позиции курсора
                          вход:  BH = номер видео страницы
                          AL = записываемый символ
                          CX = счетчик (сколько экземпляров символа записать)*/
                        for (uint16_t j = 0; j < CPU_CX; j++) {
                            bios_putchar(CPU_AL);

                        }

                        return;
                    case 0x0E:
                        /*0eH писать символ на активную видео страницу (эмуляция телетайпа)
                          вход:  AL = записываемый символ (использует существующий атрибут)
                          BL = цвет переднего плана (для графических режимов)*/
                        bios_putchar(CPU_AL);
                        return;
#endif
            }
            break;
#if 0
            case 0x16:
                switch (CPU_AH) {
                    case 0x10:
                    case 0x00:
                        /*00H читать (ожидать) следующую нажатую клавишу
                        выход: AL = ASCII символ (если AL=0, AH содержит расширенный код ASCII )
                              AH = сканкод  или расширенный код ASCII*/
                        CPU_AX = kbhit() ? getch() : 0; //kbd_get_buffer(1);
    //                    CPU_AL = 0;
    //                    CPU_AH =  _kbhit() ? getch() : 0; //kbd_get_buffer(1);

                        return;

                    case 0x11:
                    case 0x01:
                        CPU_AX = kbhit() ? getch() : 0;//kbd_get_buffer(0);
    //                    CPU_AL =  _kbhit() ? getch() : 0;
    //                    CPU_AH =  _kbhit() ? getch() : 0; //kbd_get_buffer(1);

                        if (CPU_AX)
                            CPU_FL_ZF = 0;
                        else
                            CPU_FL_ZF = 1;
                        return;
                }
                break;
#endif
        /* case 0x19: // bootstrap
             didbootstrap = 1;
             break;
             printf("Bootstrap\r\n");

             if (bootdrive < 255) { // read first sector of boot drive into
                 // 07C0:0000 and execute it
                 CPU_DL = bootdrive;
                 bios_read_boot_sector(bootdrive, 0, 0x7C00);
                 if (cf) {
                     fprintf(stderr, "BOOT: cannot read boot record of drive %02X! Trying ROM basic instead!\n",
                             bootdrive);
                     CPU_CS = 0xF600;
                     ip = 0;
                 } else {
                     CPU_CS = 0x0000;
                     ip = 0x7C00;
                     printf("BOOT: executing boot record at %04X:%04X\n", CPU_CS, ip);
                 }
             } else {
                 CPU_CS = 0xF600; // start ROM BASIC at bootstrap if requested
                 ip = 0x0000;
             }
             return;*/

        case 0x19:
            insertdisk(0, fdd0_sz(), fdd0_rom(), "\\XT\\fdd0.img");
            insertdisk(1, fdd1_sz(), fdd1_rom(), "\\XT\\fdd1.img");
#if PICO_ON_DEVICE
            insertdisk(128, 0, NULL, "\\XT\\hdd.img");
            keyboard_send(0xFF);
#else
            insertdisk(128, 0, NULL, "hdd.img");
#endif
            break;
        case 0x13:
        case 0xFD:
            diskhandler();
            return;
    }
    push(makeflagsword());
    push(CPU_CS);
    push(ip);
    CPU_CS = getmem16(0, (uint16_t) intnum * 4 + 2);
    ip = getmem16(0, (uint16_t) intnum * 4);
    ifl = 0;
    tf = 0;
}


uint8_t op_grp2_8(uint8_t cnt) {
    uint16_t s;
    uint16_t shift;
    uint16_t oldcf;
    uint16_t msb;

    s = oper1b;
    oldcf = cf;
    switch (reg) {
        case 0: /* ROL r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                if (s & 0x80) {
                    cf = 1;
                }
                else {
                    cf = 0;
                }

                s = s << 1;
                s = s | cf;
            }

            if (cnt == 1) {
                //of = cf ^ ( (s >> 7) & 1);
                if ((s & 0x80) && cf) of = 1;
                else of = 0;
            }
            else of = 0;
            break;

        case 1: /* ROR r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                cf = s & 1;
                s = (s >> 1) | (cf << 7);
            }

            if (cnt == 1) {
                of = (s >> 7) ^ ((s >> 6) & 1);
            }
            break;

        case 2: /* RCL r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                oldcf = cf;
                if (s & 0x80) {
                    cf = 1;
                }
                else {
                    cf = 0;
                }

                s = s << 1;
                s = s | oldcf;
            }

            if (cnt == 1) {
                of = cf ^ ((s >> 7) & 1);
            }
            break;

        case 3: /* RCR r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                oldcf = cf;
                cf = s & 1;
                s = (s >> 1) | (oldcf << 7);
            }

            if (cnt == 1) {
                of = (s >> 7) ^ ((s >> 6) & 1);
            }
            break;

        case 4:
        case 6: /* SHL r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                if (s & 0x80) {
                    cf = 1;
                }
                else {
                    cf = 0;
                }

                s = (s << 1) & 0xFF;
            }

            if ((cnt == 1) && (cf == (s >> 7))) {
                of = 0;
            }
            else {
                of = 1;
            }

            flag_szp8((uint8_t)s);
            break;

        case 5: /* SHR r/m8 */
            if ((cnt == 1) && (s & 0x80)) {
                of = 1;
            }
            else {
                of = 0;
            }

            for (shift = 1; shift <= cnt; shift++) {
                cf = s & 1;
                s = s >> 1;
            }

            flag_szp8((uint8_t)s);
            break;

        case 7: /* SAR r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                msb = s & 0x80;
                cf = s & 1;
                s = (s >> 1) | msb;
            }

            of = 0;
            flag_szp8((uint8_t)s);
            break;
    }

    return s & 0xFF;
}

uint16_t op_grp2_16(uint8_t cnt) {
    uint32_t s;
    uint32_t shift;
    uint32_t oldcf;
    uint32_t msb;

    s = oper1;
    oldcf = cf;
    switch (reg) {
        case 0: /* ROL r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                if (s & 0x8000) {
                    cf = 1;
                }
                else {
                    cf = 0;
                }

                s = s << 1;
                s = s | cf;
            }

            if (cnt == 1) {
                of = cf ^ ((s >> 15) & 1);
            }
            break;

        case 1: /* ROR r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                cf = s & 1;
                s = (s >> 1) | (cf << 15);
            }

            if (cnt == 1) {
                of = (s >> 15) ^ ((s >> 14) & 1);
            }
            break;

        case 2: /* RCL r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                oldcf = cf;
                if (s & 0x8000) {
                    cf = 1;
                }
                else {
                    cf = 0;
                }

                s = s << 1;
                s = s | oldcf;
            }

            if (cnt == 1) {
                of = cf ^ ((s >> 15) & 1);
            }
            break;

        case 3: /* RCR r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                oldcf = cf;
                cf = s & 1;
                s = (s >> 1) | (oldcf << 15);
            }

            if (cnt == 1) {
                of = (s >> 15) ^ ((s >> 14) & 1);
            }
            break;

        case 4:
        case 6: /* SHL r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                if (s & 0x8000) {
                    cf = 1;
                }
                else {
                    cf = 0;
                }

                s = (s << 1) & 0xFFFF;
            }

            if ((cnt == 1) && (cf == (s >> 15))) {
                of = 0;
            }
            else {
                of = 1;
            }

            flag_szp16((uint16_t)s);
            break;

        case 5: /* SHR r/m8 */
            if ((cnt == 1) && (s & 0x8000)) {
                of = 1;
            }
            else {
                of = 0;
            }

            for (shift = 1; shift <= cnt; shift++) {
                cf = s & 1;
                s = s >> 1;
            }

            flag_szp16((uint16_t)s);
            break;

        case 7: /* SAR r/m8 */
            for (shift = 1; shift <= cnt; shift++) {
                msb = s & 0x8000;
                cf = s & 1;
                s = (s >> 1) | msb;
            }

            of = 0;
            flag_szp16((uint16_t)s);
            break;
    }

    return (uint16_t)s & 0xFFFF;
}

void op_div8(uint16_t valdiv, uint8_t divisor) {
    if (divisor == 0) {
        intcall86(0);
        return;
    }

    if ((valdiv / (uint16_t)divisor) > 0xFF) {
        intcall86(0);
        return;
    }

    regs.byteregs[regah] = valdiv % (uint16_t)divisor;
    CPU_AL = valdiv / (uint16_t)divisor;
}

void op_idiv8(uint16_t valdiv, uint8_t divisor) {
    uint16_t s1;
    uint16_t s2;
    uint16_t d1;
    uint16_t d2;
    int sign;

    if (divisor == 0) {
        intcall86(0);
        return;
    }

    s1 = valdiv;
    s2 = divisor;
    sign = (((s1 ^ s2) & 0x8000) != 0);
    s1 = (s1 < 0x8000) ? s1 : ((~s1 + 1) & 0xffff);
    s2 = (s2 < 0x8000) ? s2 : ((~s2 + 1) & 0xffff);
    d1 = s1 / s2;
    d2 = s1 % s2;
    if (d1 & 0xFF00) {
        intcall86(0);
        return;
    }

    if (sign) {
        d1 = (~d1 + 1) & 0xff;
        d2 = (~d2 + 1) & 0xff;
    }

    regs.byteregs[regah] = (uint8_t)d2;
    CPU_AL = (uint8_t)d1;
}

void op_grp3_8() {
    oper1 = signext(oper1b);
    oper2 = signext(oper2b);
    switch (reg) {
        case 0:
        case 1: /* TEST */
            flag_log8(oper1b & getmem8(CPU_CS, ip));
            StepIP(1);
            break;

        case 2: /* NOT */
            res8 = ~oper1b;
            break;

        case 3: /* NEG */
            res8 = (~oper1b) + 1;
            flag_sub8(0, oper1b);
            if (res8 == 0) {
                cf = 0;
            }
            else {
                cf = 1;
            }
            break;

        case 4: /* MUL */
            temp1 = (uint32_t)oper1b * (uint32_t)CPU_AL;
            CPU_AX = temp1 & 0xFFFF;
            flag_szp8((uint8_t)temp1);
            if (regs.byteregs[regah]) {
                cf = 1;
                of = 1;
            }
            else {
                cf = 0;
                of = 0;
            }
            break;

        case 5: /* IMUL */
            oper1 = signext(oper1b);
            temp1 = signext(CPU_AL);
            temp2 = oper1;
            if ((temp1 & 0x80) == 0x80) {
                temp1 = temp1 | 0xFFFFFF00;
            }

            if ((temp2 & 0x80) == 0x80) {
                temp2 = temp2 | 0xFFFFFF00;
            }

            temp3 = (temp1 * temp2) & 0xFFFF;
            CPU_AX = temp3 & 0xFFFF;
            if (regs.byteregs[regah]) {
                cf = 1;
                of = 1;
            }
            else {
                cf = 0;
                of = 0;
            }
            break;

        case 6: /* DIV */
            op_div8(CPU_AX, oper1b);
            break;

        case 7: /* IDIV */
            op_idiv8(CPU_AX, oper1b);
            break;
    }
}

void op_div16(uint32_t valdiv, uint16_t divisor) {
    if (divisor == 0) {
        intcall86(0);
        return;
    }

    if ((valdiv / (uint32_t)divisor) > 0xFFFF) {
        intcall86(0);
        return;
    }

    CPU_DX = valdiv % (uint32_t)divisor;
    CPU_AX = valdiv / (uint32_t)divisor;
}

void op_idiv16(uint32_t valdiv, uint16_t divisor) {
    uint32_t d1;
    uint32_t d2;
    uint32_t s1;
    uint32_t s2;
    int sign;

    if (divisor == 0) {
        intcall86(0);
        return;
    }

    s1 = valdiv;
    s2 = divisor;
    s2 = (s2 & 0x8000) ? (s2 | 0xffff0000) : s2;
    sign = (((s1 ^ s2) & 0x80000000) != 0);
    s1 = (s1 < 0x80000000) ? s1 : ((~s1 + 1) & 0xffffffff);
    s2 = (s2 < 0x80000000) ? s2 : ((~s2 + 1) & 0xffffffff);
    d1 = s1 / s2;
    d2 = s1 % s2;
    if (d1 & 0xFFFF0000) {
        intcall86(0);
        return;
    }

    if (sign) {
        d1 = (~d1 + 1) & 0xffff;
        d2 = (~d2 + 1) & 0xffff;
    }

    CPU_AX = d1;
    CPU_DX = d2;
}

void op_grp3_16() {
    switch (reg) {
        case 0:
        case 1: /* TEST */
            flag_log16(oper1 & getmem16(CPU_CS, ip));
            StepIP(2);
            break;

        case 2: /* NOT */
            res16 = ~oper1;
            break;

        case 3: /* NEG */
            res16 = (~oper1) + 1;
            flag_sub16(0, oper1);
            if (res16) {
                cf = 1;
            }
            else {
                cf = 0;
            }
            break;

        case 4: /* MUL */
            temp1 = (uint32_t)oper1 * (uint32_t)CPU_AX;
            CPU_AX = temp1 & 0xFFFF;
            CPU_DX = temp1 >> 16;
            flag_szp16((uint16_t)temp1);
            if (CPU_DX) {
                cf = 1;
                of = 1;
            }
            else {
                cf = 0;
                of = 0;
            }
            break;

        case 5: /* IMUL */
            temp1 = CPU_AX;
            temp2 = oper1;
            if (temp1 & 0x8000) {
                temp1 |= 0xFFFF0000;
            }

            if (temp2 & 0x8000) {
                temp2 |= 0xFFFF0000;
            }

            temp3 = temp1 * temp2;
            CPU_AX = temp3 & 0xFFFF; /* into register ax */
            CPU_DX = temp3 >> 16; /* into register dx */
            if (CPU_DX) {
                cf = 1;
                of = 1;
            }
            else {
                cf = 0;
                of = 0;
            }
            break;

        case 6: /* DIV */
            op_div16(((uint32_t)CPU_DX << 16) + CPU_AX, oper1);
            break;

        case 7: /* DIV */
            op_idiv16(((uint32_t)CPU_DX << 16) + CPU_AX, oper1);
            break;
    }
}

void op_grp5() {
    switch (reg) {
        case 0: /* INC Ev */
            oper2 = 1;
            tempcf = cf;
            op_add16();
            cf = tempcf;
            writerm16(rm, res16);
            break;

        case 1: /* DEC Ev */
            oper2 = 1;
            tempcf = cf;
            op_sub16();
            cf = tempcf;
            writerm16(rm, res16);
            break;

        case 2: /* CALL Ev */
            push(ip);
            ip = oper1;
            break;

        case 3: /* CALL Mp */
            push(CPU_CS);
            push(ip);
            getea(rm);
            ip = (uint16_t)read86(ea) + (uint16_t)read86(ea + 1) * 256;
            CPU_CS = (uint16_t)read86(ea + 2) + (uint16_t)read86(ea + 3) * 256;
            break;

        case 4: /* JMP Ev */
            ip = oper1;
            break;

        case 5: /* JMP Mp */
            getea(rm);
            ip = (uint16_t)read86(ea) + (uint16_t)read86(ea + 1) * 256;
            CPU_CS = (uint16_t)read86(ea + 2) + (uint16_t)read86(ea + 3) * 256;
            break;

        case 6: /* PUSH Ev */
            push(oper1);
            break;
    }
}

#if !PICO_ON_DEVICE
int hijacked_input = 0;
static uint8_t keydown[0x100];

static int translatescancode_from_sdl(SDL_Keycode keyval) {
    //printf("translatekey for 0x%04X %s\n", keyval, SDL_GetKeyName(keyval));
    switch (keyval) {
        case SDLK_ESCAPE:
            return 0x01;        // escape
        case 0x30:
            return 0x0B;        // zero
        case 0x31:                    // numeric keys 1-9
        case 0x32:
        case 0x33:
        case 0x34:
        case 0x35:
        case 0x36:
        case 0x37:
        case 0x38:
        case 0x39:
            return keyval - 0x2F;
        case 0x2D:
            return 0x0C;
        case 0x3D:
            return 0x0D;
        case SDLK_BACKSPACE:
            return 0x0E;
        case SDLK_TAB:
            return 0x0F;
        case 0x71:
            return 0x10;
        case 0x77:
            return 0x11;
        case 0x65:
            return 0x12;
        case 0x72:
            return 0x13;
        case 0x74:
            return 0x14;
        case 0x79:
            return 0x15;
        case 0x75:
            return 0x16;
        case 0x69:
            return 0x17;
        case 0x6F:
            return 0x18;
        case 0x70:
            return 0x19;
        case 0x5B:
            return 0x1A;
        case 0x5D:
            return 0x1B;
        case SDLK_KP_ENTER:
        case SDLK_RETURN:
        case SDLK_RETURN2:
            return 0x1C;
        case SDLK_RCTRL:
        case SDLK_LCTRL:
            return 0x1D;
        case 0x61:
            return 0x1E;
        case 0x73:
            return 0x1F;
        case 0x64:
            return 0x20;
        case 0x66:
            return 0x21;
        case 0x67:
            return 0x22;
        case 0x68:
            return 0x23;
        case 0x6A:
            return 0x24;
        case 0x6B:
            return 0x25;
        case 0x6C:
            return 0x26;
        case 0x3B:
            return 0x27;
        case 0x27:
            return 0x28;
        case 0x60:
            return 0x29;
        case SDLK_LSHIFT:
            return 0x2A;
        case 0x5C:
            return 0x2B;
        case 0x7A:
            return 0x2C;
        case 0x78:
            return 0x2D;
        case 0x63:
            return 0x2E;
        case 0x76:
            return 0x2F;
        case 0x62:
            return 0x30;
        case 0x6E:
            return 0x31;
        case 0x6D:
            return 0x32;
        case 0x2C:
            return 0x33;
        case 0x2E:
            return 0x34;
        case 0x2F:
            return 0x35;
        case SDLK_RSHIFT:
            return 0x36;
        case SDLK_PRINTSCREEN:
            return 0x37;
        case SDLK_RALT:
        case SDLK_LALT:
            return 0x38;
        case SDLK_SPACE:
            return 0x39;
        case SDLK_CAPSLOCK:
            return 0x3A;
        case SDLK_F1:
            return 0x3B;    // F1
        case SDLK_F2:
            return 0x3C;    // F2
        case SDLK_F3:
            return 0x3D;    // F3
        case SDLK_F4:
            return 0x3E;    // F4
        case SDLK_F5:
            return 0x3F;    // F5
        case SDLK_F6:
            return 0x40;    // F6
        case SDLK_F7:
            return 0x41;    // F7
        case SDLK_F8:
            return 0x42;    // F8
        case SDLK_F9:
            return 0x43;    // F9
        case SDLK_F10:
            return 0x44;    // F10
        case SDLK_NUMLOCKCLEAR:
            return 0x45;    // numlock
        case SDLK_SCROLLLOCK:
            return 0x46;    // scroll lock
        case SDLK_KP_7:
        case SDLK_HOME:
            return 0x47;
        case SDLK_KP_8:
        case SDLK_UP:
            return 0x48;
        case SDLK_KP_9:
        case SDLK_PAGEUP:
            return 0x49;
        case SDLK_KP_MINUS:
            return 0x4A;
        case SDLK_KP_4:
        case SDLK_LEFT:
            return 0x4B;
        case SDLK_KP_5:
            return 0x4C;
        case SDLK_KP_6:
        case SDLK_RIGHT:
            return 0x4D;
        case SDLK_KP_PLUS:
            return 0x4E;
        case SDLK_KP_1:
        case SDLK_END:
            return 0x4F;
        case SDLK_KP_2:
        case SDLK_DOWN:
            return 0x50;
        case SDLK_KP_3:
        case SDLK_PAGEDOWN:
            return 0x51;
        case SDLK_KP_0:
        case SDLK_INSERT:
            return 0x52;
        case SDLK_KP_PERIOD:
        case SDLK_DELETE:
            return 0x53;
        default:
            return -1;    // *** UNSUPPORTED KEY ***
    }
}

void handleinput(void) {
    SDL_Event event;
    int mx = 0, my = 0;
    uint8_t tempbuttons;
    int translated_key;
    if (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_KEYDOWN:
                translated_key = translatescancode_from_sdl(event.key.keysym.sym);
                if (translated_key >= 0) {
                    if (!hijacked_input) {
                        portram[0x60] = translated_key;
                        portram[0x64] |= 2;
                        doirq(1);
                    }
                    //printf("%02X\n", translatescancode_from_sdl(event.key.keysym.sym));
                    keydown[translated_key] = 1;
                } else if (!hijacked_input)
                    printf("INPUT: Unsupported key: %s [%d]\n", SDL_GetKeyName(event.key.keysym.sym),
                           event.key.keysym.sym);
                break;
            case SDL_KEYUP:
                translated_key = translatescancode_from_sdl(event.key.keysym.sym);
                if (translated_key >= 0) {
                    if (!hijacked_input) {
                        portram[0x60] = translated_key | 0x80;
                        portram[0x64] |= 2;
                        doirq(1);
                    }
                    keydown[translated_key] = 0;
                }
                break;
            case SDL_QUIT:
                SDL_Quit();
                break;
            default:
                break;
        }
    }
}

#else


extern void ps2poll();

#endif

void __inline exec86(uint32_t execloops) {
    uint8_t docontinue;
    static uint16_t firstip;
    static uint16_t trap_toggle = 0;

    //counterticks = (uint64_t) ( (double) timerfreq / (double) 65536.0);
    tickssource();
    for (uint32_t loopcount = 0; loopcount < execloops; loopcount++) {
        //if ((totalexec & 256) == 0)
        if (trap_toggle) {
            intcall86(1);
        }

        trap_toggle = tf;

        if (!trap_toggle && (ifl && (i8259.irr & (~i8259.imr)))) {
            intcall86(nextintr()); // get next interrupt from the i8259, if any d
        }

        reptype = 0;
        segoverride = 0;
        useseg = CPU_DS;
        docontinue = 0;
        firstip = ip;
        /*
                if ((CPU_CS == 0xF000) && (ip == 0xE066)) {
                    printf("didbootsreap\r\n");
                    didbootstrap = 0; //detect if we hit the BIOS entry point to clear didbootstrap because we've rebooted
                }*/

        while (!docontinue) {
            CPU_CS = CPU_CS & 0xFFFF;
            ip = ip & 0xFFFF;
            savecs = CPU_CS;
            saveip = ip;
            opcode = getmem8(CPU_CS, ip);
            StepIP(1);

            switch (opcode) {
                /* segment prefix check */
                case 0x2E: /* segment CPU_CS */
                    useseg = CPU_CS;
                    segoverride = 1;
                    break;

                case 0x3E: /* segment CPU_DS */
                    useseg = CPU_DS;
                    segoverride = 1;
                    break;

                case 0x26: /* segment CPU_ES */
                    useseg = CPU_ES;
                    segoverride = 1;
                    break;

                case 0x36: /* segment CPU_SS */
                    useseg = CPU_SS;
                    segoverride = 1;
                    break;

                /* repetition prefix check */
                case 0xF3: /* REP/REPE/REPZ */
                    reptype = 1;
                    break;

                case 0xF2: /* REPNE/REPNZ */
                    reptype = 2;
                    break;

                default:
                    docontinue = 1;
                    break;
            }
        }

        totalexec++;

        switch (opcode) {
            case 0x0: /* 00 ADD Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_add8();
                writerm8(rm, res8);
                break;

            case 0x1: /* 01 ADD Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_add16();
                writerm16(rm, res16);
                break;

            case 0x2: /* 02 ADD Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_add8();
                putreg8(reg, res8);
                break;

            case 0x3: /* 03 ADD Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_add16();
                putreg16(reg, res16);
                break;

            case 0x4: /* 04 ADD CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                op_add8();
                CPU_AL = res8;
                break;

            case 0x5: /* 05 ADD eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                op_add16();
                CPU_AX = res16;
                break;

            case 0x6: /* 06 PUSH CPU_ES */
                push(CPU_ES);
                break;

            case 0x7: /* 07 POP CPU_ES */
                CPU_ES = pop();
                break;

            case 0x8: /* 08 OR Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_or8();
                writerm8(rm, res8);
                break;

            case 0x9: /* 09 OR Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_or16();
                writerm16(rm, res16);
                break;

            case 0xA: /* 0A OR Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_or8();
                putreg8(reg, res8);
                break;

            case 0xB: /* 0B OR Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_or16();
                if ((oper1 == 0xF802) && (oper2 == 0xF802)) {
                    sf = 0; /* cheap hack to make Wolf 3D think we're a 286 so it plays */
                }

                putreg16(reg, res16);
                break;

            case 0xC: /* 0C OR CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                op_or8();
                CPU_AL = res8;
                break;

            case 0xD: /* 0D OR eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                op_or16();
                CPU_AX = res16;
                break;

            case 0xE: /* 0E PUSH CPU_CS */
                push(CPU_CS);
                break;

            case 0xF: //0F POP CS only the 8086/8088 does this.
                CPU_CS = pop();
                break;

            case 0x10: /* 10 ADC Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_adc8();
                writerm8(rm, res8);
                break;

            case 0x11: /* 11 ADC Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_adc16();
                writerm16(rm, res16);
                break;

            case 0x12: /* 12 ADC Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_adc8();
                putreg8(reg, res8);
                break;

            case 0x13: /* 13 ADC Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_adc16();
                putreg16(reg, res16);
                break;

            case 0x14: /* 14 ADC CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                op_adc8();
                CPU_AL = res8;
                break;

            case 0x15: /* 15 ADC eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                op_adc16();
                CPU_AX = res16;
                break;

            case 0x16: /* 16 PUSH CPU_SS */
                push(CPU_SS);
                break;

            case 0x17: /* 17 POP CPU_SS */
                CPU_SS = pop();
                break;

            case 0x18: /* 18 SBB Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_sbb8();
                writerm8(rm, res8);
                break;

            case 0x19: /* 19 SBB Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_sbb16();
                writerm16(rm, res16);
                break;

            case 0x1A: /* 1A SBB Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_sbb8();
                putreg8(reg, res8);
                break;

            case 0x1B: /* 1B SBB Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_sbb16();
                putreg16(reg, res16);
                break;

            case 0x1C: /* 1C SBB CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                op_sbb8();
                CPU_AL = res8;
                break;

            case 0x1D: /* 1D SBB eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                op_sbb16();
                CPU_AX = res16;
                break;

            case 0x1E: /* 1E PUSH CPU_DS */
                push(CPU_DS);
                break;

            case 0x1F: /* 1F POP CPU_DS */
                CPU_DS = pop();
                break;

            case 0x20: /* 20 AND Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_and8();
                writerm8(rm, res8);
                break;

            case 0x21: /* 21 AND Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_and16();
                writerm16(rm, res16);
                break;

            case 0x22: /* 22 AND Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_and8();
                putreg8(reg, res8);
                break;

            case 0x23: /* 23 AND Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_and16();
                putreg16(reg, res16);
                break;

            case 0x24: /* 24 AND CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                op_and8();
                CPU_AL = res8;
                break;

            case 0x25: /* 25 AND eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                op_and16();
                CPU_AX = res16;
                break;

            case 0x27: /* 27 DAA */
                if (((CPU_AL & 0xF) > 9) || (af == 1)) {
                    oper1 = CPU_AL + 6;
                    CPU_AL = oper1 & 255;
                    if (oper1 & 0xFF00) {
                        cf = 1;
                    }
                    else {
                        cf = 0;
                    }

                    af = 1;
                }
                else {
                    //af = 0;
                }

                if ((CPU_AL > 0x9F) || (cf == 1)) {
                    CPU_AL = CPU_AL + 0x60;
                    cf = 1;
                }
                else {
                    //cf = 0;
                }

                CPU_AL = CPU_AL & 255;
                flag_szp8(CPU_AL);
                break;

            case 0x28: /* 28 SUB Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_sub8();
                writerm8(rm, res8);
                break;

            case 0x29: /* 29 SUB Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_sub16();
                writerm16(rm, res16);
                break;

            case 0x2A: /* 2A SUB Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_sub8();
                putreg8(reg, res8);
                break;

            case 0x2B: /* 2B SUB Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_sub16();
                putreg16(reg, res16);
                break;

            case 0x2C: /* 2C SUB CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                op_sub8();
                CPU_AL = res8;
                break;

            case 0x2D: /* 2D SUB eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                op_sub16();
                CPU_AX = res16;
                break;

            case 0x2F: /* 2F DAS */
                if (((CPU_AL & 15) > 9) || (af == 1)) {
                    oper1 = CPU_AL - 6;
                    CPU_AL = oper1 & 255;
                    if (oper1 & 0xFF00) {
                        cf = 1;
                    }
                    else {
                        cf = 0;
                    }

                    af = 1;
                }
                else {
                    af = 0;
                }

                if (((CPU_AL & 0xF0) > 0x90) || (cf == 1)) {
                    CPU_AL = CPU_AL - 0x60;
                    cf = 1;
                }
                else {
                    cf = 0;
                }

                flag_szp8(CPU_AL);
                break;

            case 0x30: /* 30 XOR Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_xor8();
                writerm8(rm, res8);
                break;

            case 0x31: /* 31 XOR Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_xor16();
                writerm16(rm, res16);
                break;

            case 0x32: /* 32 XOR Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_xor8();
                putreg8(reg, res8);
                break;

            case 0x33: /* 33 XOR Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_xor16();
                putreg16(reg, res16);
                break;

            case 0x34: /* 34 XOR CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                op_xor8();
                CPU_AL = res8;
                break;

            case 0x35: /* 35 XOR eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                op_xor16();
                CPU_AX = res16;
                break;

            case 0x37: /* 37 AAA ASCII */
                if (((CPU_AL & 0xF) > 9) || (af == 1)) {
                    CPU_AL = CPU_AL + 6;
                    regs.byteregs[regah] = regs.byteregs[regah] + 1;
                    af = 1;
                    cf = 1;
                }
                else {
                    af = 0;
                    cf = 0;
                }

                CPU_AL = CPU_AL & 0xF;
                break;

            case 0x38: /* 38 CMP Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                flag_sub8(oper1b, oper2b);
                break;

            case 0x39: /* 39 CMP Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                flag_sub16(oper1, oper2);
                break;

            case 0x3A: /* 3A CMP Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                flag_sub8(oper1b, oper2b);
                break;

            case 0x3B: /* 3B CMP Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                flag_sub16(oper1, oper2);
                break;

            case 0x3C: /* 3C CMP CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                flag_sub8(oper1b, oper2b);
                break;

            case 0x3D: /* 3D CMP eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                flag_sub16(oper1, oper2);
                break;

            case 0x3F: /* 3F AAS ASCII */
                if (((CPU_AL & 0xF) > 9) || (af == 1)) {
                    CPU_AL = CPU_AL - 6;
                    regs.byteregs[regah] = regs.byteregs[regah] - 1;
                    af = 1;
                    cf = 1;
                }
                else {
                    af = 0;
                    cf = 0;
                }

                CPU_AL = CPU_AL & 0xF;
                break;

            case 0x40: /* 40 INC eAX */
                oldcf = cf;
                oper1 = CPU_AX;
                oper2 = 1;
                op_add16();
                cf = oldcf;
                CPU_AX = res16;
                break;

            case 0x41: /* 41 INC eCX */
                oldcf = cf;
                oper1 = CPU_CX;
                oper2 = 1;
                op_add16();
                cf = oldcf;
                CPU_CX = res16;
                break;

            case 0x42: /* 42 INC eDX */
                oldcf = cf;
                oper1 = CPU_DX;
                oper2 = 1;
                op_add16();
                cf = oldcf;
                CPU_DX = res16;
                break;

            case 0x43: /* 43 INC eBX */
                oldcf = cf;
                oper1 = CPU_BX;
                oper2 = 1;
                op_add16();
                cf = oldcf;
                CPU_BX = res16;
                break;

            case 0x44: /* 44 INC eSP */
                oldcf = cf;
                oper1 = CPU_SP;
                oper2 = 1;
                op_add16();
                cf = oldcf;
                CPU_SP = res16;
                break;

            case 0x45: /* 45 INC eBP */
                oldcf = cf;
                oper1 = CPU_BP;
                oper2 = 1;
                op_add16();
                cf = oldcf;
                CPU_BP = res16;
                break;

            case 0x46: /* 46 INC eSI */
                oldcf = cf;
                oper1 = CPU_SI;
                oper2 = 1;
                op_add16();
                cf = oldcf;
                CPU_SI = res16;
                break;

            case 0x47: /* 47 INC eDI */
                oldcf = cf;
                oper1 = CPU_DI;
                oper2 = 1;
                op_add16();
                cf = oldcf;
                CPU_DI = res16;
                break;

            case 0x48: /* 48 DEC eAX */
                oldcf = cf;
                oper1 = CPU_AX;
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                CPU_AX = res16;
                break;

            case 0x49: /* 49 DEC eCX */
                oldcf = cf;
                oper1 = CPU_CX;
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                CPU_CX = res16;
                break;

            case 0x4A: /* 4A DEC eDX */
                oldcf = cf;
                oper1 = CPU_DX;
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                CPU_DX = res16;
                break;

            case 0x4B: /* 4B DEC eBX */
                oldcf = cf;
                oper1 = CPU_BX;
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                CPU_BX = res16;
                break;

            case 0x4C: /* 4C DEC eSP */
                oldcf = cf;
                oper1 = CPU_SP;
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                CPU_SP = res16;
                break;

            case 0x4D: /* 4D DEC eBP */
                oldcf = cf;
                oper1 = CPU_BP;
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                CPU_BP = res16;
                break;

            case 0x4E: /* 4E DEC eSI */
                oldcf = cf;
                oper1 = CPU_SI;
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                CPU_SI = res16;
                break;

            case 0x4F: /* 4F DEC eDI */
                oldcf = cf;
                oper1 = CPU_DI;
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                CPU_DI = res16;
                break;

            case 0x50: /* 50 PUSH eAX */
                push(CPU_AX);
                break;

            case 0x51: /* 51 PUSH eCX */
                push(CPU_CX);
                break;

            case 0x52: /* 52 PUSH eDX */
                push(CPU_DX);
                break;

            case 0x53: /* 53 PUSH eBX */
                push(CPU_BX);
                break;

            case 0x54: /* 54 PUSH eSP */
                push(CPU_SP - 2);
                break;

            case 0x55: /* 55 PUSH eBP */
                push(CPU_BP);
                break;

            case 0x56: /* 56 PUSH eSI */
                push(CPU_SI);
                break;

            case 0x57: /* 57 PUSH eDI */
                push(CPU_DI);
                break;

            case 0x58: /* 58 POP eAX */
                CPU_AX = pop();
                break;

            case 0x59: /* 59 POP eCX */
                CPU_CX = pop();
                break;

            case 0x5A: /* 5A POP eDX */
                CPU_DX = pop();
                break;

            case 0x5B: /* 5B POP eBX */
                CPU_BX = pop();
                break;

            case 0x5C: /* 5C POP eSP */
                CPU_SP = pop();
                break;

            case 0x5D: /* 5D POP eBP */
                CPU_BP = pop();
                break;

            case 0x5E: /* 5E POP eSI */
                CPU_SI = pop();
                break;

            case 0x5F: /* 5F POP eDI */
                CPU_DI = pop();
                break;

#ifndef CPU_8086
            case 0x60: /* 60 PUSHA (80186+) */
                oldsp = CPU_SP;
                push(CPU_AX);
                push(CPU_CX);
                push(CPU_DX);
                push(CPU_BX);
                push(oldsp);
                push(CPU_BP);
                push(CPU_SI);
                push(CPU_DI);
                break;

            case 0x61: /* 61 POPA (80186+) */

                CPU_DI = pop();
                CPU_SI = pop();
                CPU_BP = pop();
                uint16_t dummy = pop();
                CPU_BX = pop();
                CPU_DX = pop();
                CPU_CX = pop();
                CPU_AX = pop();
                break;

            case 0x62: /* 62 BOUND Gv, Ev (80186+) */
                modregrm();
                getea(rm);
                if (signext32(getreg16(reg)) < signext32(getmem16(ea >> 4, ea & 15))) {
                    intcall86(5); //bounds check exception
                }
                else {
                    ea += 2;
                    if (signext32(getreg16(reg)) > signext32(getmem16(ea >> 4, ea & 15))) {
                        intcall86(5); //bounds check exception
                    }
                }
                break;

            case 0x68: /* 68 PUSH Iv (80186+) */
                push(getmem16(CPU_CS, ip));
                StepIP(2);
                break;

            case 0x69: /* 69 IMUL Gv Ev Iv (80186+) */
                modregrm();
                temp1 = readrm16(rm);
                temp2 = getmem16(CPU_CS, ip);
                StepIP(2);
                if ((temp1 & 0x8000L) == 0x8000L) {
                    temp1 = temp1 | 0xFFFF0000L;
                }

                if ((temp2 & 0x8000L) == 0x8000L) {
                    temp2 = temp2 | 0xFFFF0000L;
                }

                temp3 = temp1 * temp2;
                putreg16(reg, temp3 & 0xFFFFL);
                if (temp3 & 0xFFFF0000L) {
                    cf = 1;
                    of = 1;
                }
                else {
                    cf = 0;
                    of = 0;
                }
                break;

            case 0x6A: /* 6A PUSH Ib (80186+) */
                push(getmem8(CPU_CS, ip));
                StepIP(1);
                break;

            case 0x6B: /* 6B IMUL Gv Eb Ib (80186+) */
                modregrm();
                temp1 = readrm16(rm);
                temp2 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if ((temp1 & 0x8000L) == 0x8000L) {
                    temp1 = temp1 | 0xFFFF0000L;
                }

                if ((temp2 & 0x8000L) == 0x8000L) {
                    temp2 = temp2 | 0xFFFF0000L;
                }

                temp3 = temp1 * temp2;
                putreg16(reg, temp3 & 0xFFFFL);
                if (temp3 & 0xFFFF0000L) {
                    cf = 1;
                    of = 1;
                }
                else {
                    cf = 0;
                    of = 0;
                }
                break;

            case 0x6C: /* 6E INSB */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                putmem8(useseg, CPU_SI, portin(CPU_DX));
                if (df) {
                    CPU_SI = CPU_SI - 1;
                    CPU_DI = CPU_DI - 1;
                }
                else {
                    CPU_SI = CPU_SI + 1;
                    CPU_DI = CPU_DI + 1;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0x6D: /* 6F INSW */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                putmem16(useseg, CPU_SI, portin16(CPU_DX));
                if (df) {
                    CPU_SI = CPU_SI - 2;
                    CPU_DI = CPU_DI - 2;
                }
                else {
                    CPU_SI = CPU_SI + 2;
                    CPU_DI = CPU_DI + 2;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0x6E: /* 6E OUTSB */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                portout(CPU_DX, getmem8(useseg, CPU_SI));
                if (df) {
                    CPU_SI = CPU_SI - 1;
                    CPU_DI = CPU_DI - 1;
                }
                else {
                    CPU_SI = CPU_SI + 1;
                    CPU_DI = CPU_DI + 1;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0x6F: /* 6F OUTSW */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                portout16(CPU_DX, getmem16(useseg, CPU_SI));
                if (df) {
                    CPU_SI = CPU_SI - 2;
                    CPU_DI = CPU_DI - 2;
                }
                else {
                    CPU_SI = CPU_SI + 2;
                    CPU_DI = CPU_DI + 2;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;
#endif
            case 0x70: /* 70 JO Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (of) {
                    ip = ip + temp16;
                }
                break;

            case 0x71: /* 71 JNO Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (!of) {
                    ip = ip + temp16;
                }
                break;

            case 0x72: /* 72 JB Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (cf) {
                    ip = ip + temp16;
                }
                break;

            case 0x73: /* 73 JNB Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (!cf) {
                    ip = ip + temp16;
                }
                break;

            case 0x74: /* 74 JZ Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x75: /* 75 JNZ Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (!zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x76: /* 76 JBE Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (cf || zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x77: /* 77 JA Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (!cf && !zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x78: /* 78 JS Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (sf) {
                    ip = ip + temp16;
                }
                break;

            case 0x79: /* 79 JNS Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (!sf) {
                    ip = ip + temp16;
                }
                break;

            case 0x7A: /* 7A JPE Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (pf) {
                    ip = ip + temp16;
                }
                break;

            case 0x7B: /* 7B JPO Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (!pf) {
                    ip = ip + temp16;
                }
                break;

            case 0x7C: /* 7C JL Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (sf != of) {
                    ip = ip + temp16;
                }
                break;

            case 0x7D: /* 7D JGE Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (sf == of) {
                    ip = ip + temp16;
                }
                break;

            case 0x7E: /* 7E JLE Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if ((sf != of) || zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x7F: /* 7F JG Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (!zf && (sf == of)) {
                    ip = ip + temp16;
                }
                break;

            case 0x80:
            case 0x82: /* 80/82 GRP1 Eb Ib */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                switch (reg) {
                    case 0:
                        op_add8();
                        break;
                    case 1:
                        op_or8();
                        break;
                    case 2:
                        op_adc8();
                        break;
                    case 3:
                        op_sbb8();
                        break;
                    case 4:
                        op_and8();
                        break;
                    case 5:
                        op_sub8();
                        break;
                    case 6:
                        op_xor8();
                        break;
                    case 7:
                        flag_sub8(oper1b, oper2b);
                        break;
                    default:
                        break; /* to avoid compiler warnings */
                }

                if (reg < 7) {
                    writerm8(rm, res8);
                }
                break;

            case 0x81: /* 81 GRP1 Ev Iv */
            case 0x83: /* 83 GRP1 Ev Ib */
                modregrm();
                oper1 = readrm16(rm);
                if (opcode == 0x81) {
                    oper2 = getmem16(CPU_CS, ip);
                    StepIP(2);
                }
                else {
                    oper2 = signext(getmem8(CPU_CS, ip));
                    StepIP(1);
                }

                switch (reg) {
                    case 0:
                        op_add16();
                        break;
                    case 1:
                        op_or16();
                        break;
                    case 2:
                        op_adc16();
                        break;
                    case 3:
                        op_sbb16();
                        break;
                    case 4:
                        op_and16();
                        break;
                    case 5:
                        op_sub16();
                        break;
                    case 6:
                        op_xor16();
                        break;
                    case 7:
                        flag_sub16(oper1, oper2);
                        break;
                    default:
                        break; /* to avoid compiler warnings */
                }

                if (reg < 7) {
                    writerm16(rm, res16);
                }
                break;

            case 0x84: /* 84 TEST Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                flag_log8(oper1b & oper2b);
                break;

            case 0x85: /* 85 TEST Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                flag_log16(oper1 & oper2);
                break;

            case 0x86: /* 86 XCHG Gb Eb */
                modregrm();
                oper1b = getreg8(reg);
                putreg8(reg, readrm8(rm));
                writerm8(rm, oper1b);
                break;

            case 0x87: /* 87 XCHG Gv Ev */
                modregrm();
                oper1 = getreg16(reg);
                putreg16(reg, readrm16(rm));
                writerm16(rm, oper1);
                break;

            case 0x88: /* 88 MOV Eb Gb */
                modregrm();
                writerm8(rm, getreg8(reg));
                break;

            case 0x89: /* 89 MOV Ev Gv */
                modregrm();
                writerm16(rm, getreg16(reg));
                break;

            case 0x8A: /* 8A MOV Gb Eb */
                modregrm();
                putreg8(reg, readrm8(rm));
                break;

            case 0x8B: /* 8B MOV Gv Ev */
                modregrm();
                putreg16(reg, readrm16(rm));
                break;

            case 0x8C: /* 8C MOV Ew Sw */
                modregrm();
                writerm16(rm, getsegreg(reg));
                break;

            case 0x8D: /* 8D LEA Gv M */
                modregrm();
                getea(rm);
                putreg16(reg, ea - segbase(useseg));
                break;

            case 0x8E: /* 8E MOV Sw Ew */
                modregrm();
                putsegreg(reg, readrm16(rm));
                break;

            case 0x8F: /* 8F POP Ev */
                modregrm();
                writerm16(rm, pop());
                break;

            case 0x90: /* 90 NOP */
                break;

            case 0x91: /* 91 XCHG eCX eAX */
                oper1 = CPU_CX;
                CPU_CX = CPU_AX;
                CPU_AX = oper1;
                break;

            case 0x92: /* 92 XCHG eDX eAX */
                oper1 = CPU_DX;
                CPU_DX = CPU_AX;
                CPU_AX = oper1;
                break;

            case 0x93: /* 93 XCHG eBX eAX */
                oper1 = CPU_BX;
                CPU_BX = CPU_AX;
                CPU_AX = oper1;
                break;

            case 0x94: /* 94 XCHG eSP eAX */
                oper1 = CPU_SP;
                CPU_SP = CPU_AX;
                CPU_AX = oper1;
                break;

            case 0x95: /* 95 XCHG eBP eAX */
                oper1 = CPU_BP;
                CPU_BP = CPU_AX;
                CPU_AX = oper1;
                break;

            case 0x96: /* 96 XCHG eSI eAX */
                oper1 = CPU_SI;
                CPU_SI = CPU_AX;
                CPU_AX = oper1;
                break;

            case 0x97: /* 97 XCHG eDI eAX */
                oper1 = CPU_DI;
                CPU_DI = CPU_AX;
                CPU_AX = oper1;
                break;

            case 0x98: /* 98 CBW */
                if ((CPU_AL & 0x80) == 0x80) {
                    regs.byteregs[regah] = 0xFF;
                }
                else {
                    regs.byteregs[regah] = 0;
                }
                break;

            case 0x99: /* 99 CWD */
                if ((regs.byteregs[regah] & 0x80) == 0x80) {
                    CPU_DX = 0xFFFF;
                }
                else {
                    CPU_DX = 0;
                }
                break;

            case 0x9A: /* 9A CALL Ap */
                oper1 = getmem16(CPU_CS, ip);
                StepIP(2);
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                push(CPU_CS);
                push(ip);
                ip = oper1;
                CPU_CS = oper2;
                break;

            case 0x9B: /* 9B WAIT */
                break;

            case 0x9C: /* 9C PUSHF */
                push(makeflagsword() | 0x0800);
                break;

            case 0x9D: /* 9D POPF */
                temp16 = pop();
                decodeflagsword(temp16);
                break;

            case 0x9E: /* 9E SAHF */
                decodeflagsword((makeflagsword() & 0xFF00) | regs.byteregs[regah]);
                break;

            case 0x9F: /* 9F LAHF */
                regs.byteregs[regah] = makeflagsword() & 0xFF;
                break;

            case 0xA0: /* A0 MOV CPU_AL Ob */
                CPU_AL = getmem8(useseg, getmem16(CPU_CS, ip));
                StepIP(2);
                break;

            case 0xA1: /* A1 MOV eAX Ov */
                oper1 = getmem16(useseg, getmem16(CPU_CS, ip));
                StepIP(2);
                CPU_AX = oper1;
                break;

            case 0xA2: /* A2 MOV Ob CPU_AL */
                putmem8(useseg, getmem16(CPU_CS, ip), CPU_AL);
                StepIP(2);
                break;

            case 0xA3: /* A3 MOV Ov eAX */
                putmem16(useseg, getmem16(CPU_CS, ip), CPU_AX);
                StepIP(2);
                break;

            case 0xA4: /* A4 MOVSB */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                putmem8(CPU_ES, CPU_DI, getmem8(useseg, CPU_SI));
                if (df) {
                    CPU_SI = CPU_SI - 1;
                    CPU_DI = CPU_DI - 1;
                }
                else {
                    CPU_SI = CPU_SI + 1;
                    CPU_DI = CPU_DI + 1;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xA5: /* A5 MOVSW */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                putmem16(CPU_ES, CPU_DI, getmem16(useseg, CPU_SI));
                if (df) {
                    CPU_SI = CPU_SI - 2;
                    CPU_DI = CPU_DI - 2;
                }
                else {
                    CPU_SI = CPU_SI + 2;
                    CPU_DI = CPU_DI + 2;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xA6: /* A6 CMPSB */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                oper1b = getmem8(useseg, CPU_SI);
                oper2b = getmem8(CPU_ES, CPU_DI);
                if (df) {
                    CPU_SI = CPU_SI - 1;
                    CPU_DI = CPU_DI - 1;
                }
                else {
                    CPU_SI = CPU_SI + 1;
                    CPU_DI = CPU_DI + 1;
                }

                flag_sub8(oper1b, oper2b);
                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                if ((reptype == 1) && !zf) {
                    break;
                }
                else if ((reptype == 2) && (zf == 1)) {
                    break;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xA7: /* A7 CMPSW */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                oper1 = getmem16(useseg, CPU_SI);
                oper2 = getmem16(CPU_ES, CPU_DI);
                if (df) {
                    CPU_SI = CPU_SI - 2;
                    CPU_DI = CPU_DI - 2;
                }
                else {
                    CPU_SI = CPU_SI + 2;
                    CPU_DI = CPU_DI + 2;
                }

                flag_sub16(oper1, oper2);
                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                if ((reptype == 1) && !zf) {
                    break;
                }

                if ((reptype == 2) && (zf == 1)) {
                    break;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xA8: /* A8 TEST CPU_AL Ib */
                oper1b = CPU_AL;
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                flag_log8(oper1b & oper2b);
                break;

            case 0xA9: /* A9 TEST eAX Iv */
                oper1 = CPU_AX;
                oper2 = getmem16(CPU_CS, ip);
                StepIP(2);
                flag_log16(oper1 & oper2);
                break;

            case 0xAA: /* AA STOSB */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                putmem8(CPU_ES, CPU_DI, CPU_AL);
                if (df) {
                    CPU_DI = CPU_DI - 1;
                }
                else {
                    CPU_DI = CPU_DI + 1;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAB: /* AB STOSW */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                putmem16(CPU_ES, CPU_DI, CPU_AX);
                if (df) {
                    CPU_DI = CPU_DI - 2;
                }
                else {
                    CPU_DI = CPU_DI + 2;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAC: /* AC LODSB */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                CPU_AL = getmem8(useseg, CPU_SI);
                if (df) {
                    CPU_SI = CPU_SI - 1;
                }
                else {
                    CPU_SI = CPU_SI + 1;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAD: /* AD LODSW */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                oper1 = getmem16(useseg, CPU_SI);
                CPU_AX = oper1;
                if (df) {
                    CPU_SI = CPU_SI - 2;
                }
                else {
                    CPU_SI = CPU_SI + 2;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAE: /* AE SCASB */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                oper1b = CPU_AL;
                oper2b = getmem8(CPU_ES, CPU_DI);
                flag_sub8(oper1b, oper2b);
                if (df) {
                    CPU_DI = CPU_DI - 1;
                }
                else {
                    CPU_DI = CPU_DI + 1;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                if ((reptype == 1) && !zf) {
                    break;
                }
                else if ((reptype == 2) && (zf == 1)) {
                    break;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAF: /* AF SCASW */
                if (reptype && (CPU_CX == 0)) {
                    break;
                }

                oper1 = CPU_AX;
                oper2 = getmem16(CPU_ES, CPU_DI);
                flag_sub16(oper1, oper2);
                if (df) {
                    CPU_DI = CPU_DI - 2;
                }
                else {
                    CPU_DI = CPU_DI + 2;
                }

                if (reptype) {
                    CPU_CX = CPU_CX - 1;
                }

                if ((reptype == 1) && !zf) {
                    break;
                }
                else if ((reptype == 2) & (zf == 1)) {
                    break;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xB0: /* B0 MOV CPU_AL Ib */
                CPU_AL = getmem8(CPU_CS, ip);
                StepIP(1);
                break;

            case 0xB1: /* B1 MOV regs.byteregs[regcl] Ib */
                regs.byteregs[regcl] = getmem8(CPU_CS, ip);
                StepIP(1);
                break;

            case 0xB2: /* B2 MOV regs.byteregs[regdl] Ib */
                regs.byteregs[regdl] = getmem8(CPU_CS, ip);
                StepIP(1);
                break;

            case 0xB3: /* B3 MOV regs.byteregs[regbl] Ib */
                regs.byteregs[regbl] = getmem8(CPU_CS, ip);
                StepIP(1);
                break;

            case 0xB4: /* B4 MOV regs.byteregs[regah] Ib */
                regs.byteregs[regah] = getmem8(CPU_CS, ip);
                StepIP(1);
                break;

            case 0xB5: /* B5 MOV regs.byteregs[regch] Ib */
                regs.byteregs[regch] = getmem8(CPU_CS, ip);
                StepIP(1);
                break;

            case 0xB6: /* B6 MOV regs.byteregs[regdh] Ib */
                regs.byteregs[regdh] = getmem8(CPU_CS, ip);
                StepIP(1);
                break;

            case 0xB7: /* B7 MOV regs.byteregs[regbh] Ib */
                regs.byteregs[regbh] = getmem8(CPU_CS, ip);
                StepIP(1);
                break;

            case 0xB8: /* B8 MOV eAX Iv */
                oper1 = getmem16(CPU_CS, ip);
                StepIP(2);
                CPU_AX = oper1;
                break;

            case 0xB9: /* B9 MOV eCX Iv */
                oper1 = getmem16(CPU_CS, ip);
                StepIP(2);
                CPU_CX = oper1;
                break;

            case 0xBA: /* BA MOV eDX Iv */
                oper1 = getmem16(CPU_CS, ip);
                StepIP(2);
                CPU_DX = oper1;
                break;

            case 0xBB: /* BB MOV eBX Iv */
                oper1 = getmem16(CPU_CS, ip);
                StepIP(2);
                CPU_BX = oper1;
                break;

            case 0xBC: /* BC MOV eSP Iv */
                CPU_SP = getmem16(CPU_CS, ip);
                StepIP(2);
                break;

            case 0xBD: /* BD MOV eBP Iv */
                CPU_BP = getmem16(CPU_CS, ip);
                StepIP(2);
                break;

            case 0xBE: /* BE MOV eSI Iv */
                CPU_SI = getmem16(CPU_CS, ip);
                StepIP(2);
                break;

            case 0xBF: /* BF MOV eDI Iv */
                CPU_DI = getmem16(CPU_CS, ip);
                StepIP(2);
                break;

            case 0xC0: /* C0 GRP2 byte imm8 (80186+) */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getmem8(CPU_CS, ip);
                StepIP(1);
                writerm8(rm, op_grp2_8(oper2b));
                break;

            case 0xC1: /* C1 GRP2 word imm8 (80186+) */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getmem8(CPU_CS, ip);
                StepIP(1);
                writerm16(rm, op_grp2_16((uint8_t)oper2));
                break;

            case 0xC2: /* C2 RET Iw */
                oper1 = getmem16(CPU_CS, ip);
                ip = pop();
                CPU_SP = CPU_SP + oper1;
                break;

            case 0xC3: /* C3 RET */
                ip = pop();
                break;

            case 0xC4: /* C4 LES Gv Mp */
                modregrm();
                getea(rm);
                putreg16(reg, read86(ea) + read86(ea + 1) * 256);
                CPU_ES = read86(ea + 2) + read86(ea + 3) * 256;
                break;

            case 0xC5: /* C5 LDS Gv Mp */
                modregrm();
                getea(rm);
                putreg16(reg, read86(ea) + read86(ea + 1) * 256);
                CPU_DS = read86(ea + 2) + read86(ea + 3) * 256;
                break;

            case 0xC6: /* C6 MOV Eb Ib */
                modregrm();
                writerm8(rm, getmem8(CPU_CS, ip));
                StepIP(1);
                break;

            case 0xC7: /* C7 MOV Ev Iv */
                modregrm();
                writerm16(rm, getmem16(CPU_CS, ip));
                StepIP(2);
                break;

            case 0xC8: /* C8 ENTER (80186+) */
                stacksize = getmem16(CPU_CS, ip);
                StepIP(2);
                nestlev = getmem8(CPU_CS, ip);
                StepIP(1);
                push(CPU_BP);
                frametemp = CPU_SP;
                if (nestlev) {
                    for (temp16 = 1; temp16 < nestlev; temp16++) {
                        CPU_BP = CPU_BP - 2;
                        push(CPU_BP);
                    }

                    push(CPU_SP);
                }

                CPU_BP = frametemp;
                CPU_SP = CPU_BP - stacksize;

                break;

            case 0xC9: /* C9 LEAVE (80186+) */
                CPU_SP = CPU_BP;
                CPU_BP = pop();
                break;

            case 0xCA: /* CA RETF Iw */
                oper1 = getmem16(CPU_CS, ip);
                ip = pop();
                CPU_CS = pop();
                CPU_SP = CPU_SP + oper1;
                break;

            case 0xCB: /* CB RETF */
                ip = pop();;
                CPU_CS = pop();
                break;

            case 0xCC: /* CC INT 3 */
                intcall86(3);
                break;

            case 0xCD: /* CD INT Ib */
                oper1b = getmem8(CPU_CS, ip);
                StepIP(1);
                intcall86(oper1b);
                break;

            case 0xCE: /* CE INTO */
                if (of) {
                    intcall86(4);
                }
                break;

            case 0xCF: /* CF IRET */
                ip = pop();
                CPU_CS = pop();
                decodeflagsword(pop());

            /*
         * if (net.enabled) net.canrecv = 1;
         */
                break;

            case 0xD0: /* D0 GRP2 Eb 1 */
                modregrm();
                oper1b = readrm8(rm);
                writerm8(rm, op_grp2_8(1));
                break;

            case 0xD1: /* D1 GRP2 Ev 1 */
                modregrm();
                oper1 = readrm16(rm);
                writerm16(rm, op_grp2_16(1));
                break;

            case 0xD2: /* D2 GRP2 Eb regs.byteregs[regcl] */
                modregrm();
                oper1b = readrm8(rm);
                writerm8(rm, op_grp2_8(regs.byteregs[regcl]));
                break;

            case 0xD3: /* D3 GRP2 Ev regs.byteregs[regcl] */
                modregrm();
                oper1 = readrm16(rm);
                writerm16(rm, op_grp2_16(regs.byteregs[regcl]));
                break;

            case 0xD4: /* D4 AAM I0 */
                oper1 = getmem8(CPU_CS, ip);
                StepIP(1);
                if (!oper1) {
                    intcall86(0);
                    break;
                } /* division by zero */

                regs.byteregs[regah] = (CPU_AL / oper1) & 255;
                CPU_AL = (CPU_AL % oper1) & 255;
                flag_szp16(CPU_AX);
                break;

            case 0xD5: /* D5 AAD I0 */
                oper1 = getmem8(CPU_CS, ip);
                StepIP(1);
                CPU_AL = (regs.byteregs[regah] * oper1 + CPU_AL) & 255;
                regs.byteregs[regah] = 0;
                flag_szp16(regs.byteregs[regah] * oper1 + CPU_AL);
                sf = 0;
                break;

            case 0xD6: /* D6 XLAT on V20/V30, SALC on 8086/8088 */
                CPU_AL = cf ? 0xFF : 0x00;
                break;

            case 0xD7: /* D7 XLAT */
                CPU_AL = read86(useseg * 16 + (CPU_BX) + CPU_AL);
                break;

            case 0xD8:
            case 0xD9:
            case 0xDA:
            case 0xDB:
            case 0xDC:
            case 0xDE:
            case 0xDD:
            case 0xDF: /* escape to x87 FPU (unsupported) */
                modregrm();
                break;

            case 0xE0: /* E0 LOOPNZ Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                CPU_CX = CPU_CX - 1;
                if ((CPU_CX) && !zf) {
                    ip = ip + temp16;
                }
                break;

            case 0xE1: /* E1 LOOPZ Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                CPU_CX = CPU_CX - 1;
                if (CPU_CX && (zf == 1)) {
                    ip = ip + temp16;
                }
                break;

            case 0xE2: /* E2 LOOP Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                CPU_CX = CPU_CX - 1;
                if (CPU_CX) {
                    ip = ip + temp16;
                }
                break;

            case 0xE3: /* E3 JCXZ Jb */
                temp16 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                if (!CPU_CX) {
                    ip = ip + temp16;
                }
                break;

            case 0xE4: /* E4 IN CPU_AL Ib */
                oper1b = getmem8(CPU_CS, ip);
                StepIP(1);
                CPU_AL = (uint8_t)portin(oper1b);
                break;

            case 0xE5: /* E5 IN eAX Ib */
                oper1b = getmem8(CPU_CS, ip);
                StepIP(1);
                CPU_AX = portin16(oper1b);
                break;

            case 0xE6: /* E6 OUT Ib CPU_AL */
                oper1b = getmem8(CPU_CS, ip);
                StepIP(1);
                portout(oper1b, CPU_AL);
                break;

            case 0xE7: /* E7 OUT Ib eAX */
                oper1b = getmem8(CPU_CS, ip);
                StepIP(1);
                portout16(oper1b, CPU_AX);
                break;

            case 0xE8: /* E8 CALL Jv */
                oper1 = getmem16(CPU_CS, ip);
                StepIP(2);
                push(ip);
                ip = ip + oper1;
                break;

            case 0xE9: /* E9 JMP Jv */
                oper1 = getmem16(CPU_CS, ip);
                StepIP(2);
                ip = ip + oper1;
                break;

            case 0xEA: /* EA JMP Ap */
                oper1 = getmem16(CPU_CS, ip);
                StepIP(2);
                oper2 = getmem16(CPU_CS, ip);
                ip = oper1;
                CPU_CS = oper2;
                break;

            case 0xEB: /* EB JMP Jb */
                oper1 = signext(getmem8(CPU_CS, ip));
                StepIP(1);
                ip = ip + oper1;
                break;

            case 0xEC: /* EC IN CPU_AL regdx */
                oper1 = CPU_DX;
                CPU_AL = (uint8_t)portin(oper1);
                break;

            case 0xED: /* ED IN eAX regdx */
                oper1 = CPU_DX;
                CPU_AX = portin16(oper1);
                break;

            case 0xEE: /* EE OUT regdx CPU_AL */
                oper1 = CPU_DX;
                portout(oper1, CPU_AL);
                break;

            case 0xEF: /* EF OUT regdx eAX */
                oper1 = CPU_DX;
                portout16(oper1, CPU_AX);
                break;

            case 0xF0: /* F0 LOCK */
                break;

            case 0xF4: /* F4 HLT */
                //                hltstate = 1;
                break;

            case 0xF5: /* F5 CMC */
                if (!cf) {
                    cf = 1;
                }
                else {
                    cf = 0;
                }
                break;

            case 0xF6: /* F6 GRP3a Eb */
                modregrm();
                oper1b = readrm8(rm);
                op_grp3_8();
                if ((reg > 1) && (reg < 4)) {
                    writerm8(rm, res8);
                }
                break;

            case 0xF7: /* F7 GRP3b Ev */
                modregrm();
                oper1 = readrm16(rm);
                op_grp3_16();
                if ((reg > 1) && (reg < 4)) {
                    writerm16(rm, res16);
                }
                break;

            case 0xF8: /* F8 CLC */
                cf = 0;
                break;

            case 0xF9: /* F9 STC */
                cf = 1;
                break;

            case 0xFA: /* FA CLI */
                ifl = 0;
                break;

            case 0xFB: /* FB STI */
                ifl = 1;
                break;

            case 0xFC: /* FC CLD */
                df = 0;
                break;

            case 0xFD: /* FD STD */
                df = 1;
                break;

            case 0xFE: /* FE GRP4 Eb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = 1;
                if (!reg) {
                    tempcf = cf;
                    res8 = oper1b + oper2b;
                    flag_add8(oper1b, oper2b);
                    cf = tempcf;
                    writerm8(rm, res8);
                }
                else {
                    tempcf = cf;
                    res8 = oper1b - oper2b;
                    flag_sub8(oper1b, oper2b);
                    cf = tempcf;
                    writerm8(rm, res8);
                }
                break;

            case 0xFF: /* FF GRP5 Ev */
                modregrm();
                oper1 = readrm16(rm);
                op_grp5();
                break;

            default:
                intcall86(6);
                break;
        }
    }
}
