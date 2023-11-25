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
#if PICO_ON_DEVICE
__aligned(4096)
#endif
uint8_t RAM[RAM_SIZE << 10];
uint8_t VRAM[VRAM_SIZE << 10];

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

__inline void write86(uint32_t addr32, uint8_t value) {
#if PICO_ON_DEVICE
    if ((PSRAM_AVAILABLE && (addr32) < (RAM_SIZE << 10)) || addr32 < RAM_PAGE_SIZE) {
        // do not touch first page
        RAM[addr32] = value;
        return;
    }
#else
    if (addr32 < (RAM_SIZE << 10)) {
        RAM[addr32] = value;
    }
#endif
    else if (((addr32) >= 0xB8000UL) && ((addr32) < 0xC0000UL)) {
        // video RAM range
        addr32 -= 0xB8000UL;
        VRAM[addr32] = value; // 16k for graphic mode!!!
        return;
    }
#if SD_CARD_SWAP
    if ((addr32 >> 4) >= PHISICAL_EMM_SEGMENT && (addr32 << 4) < PHISICAL_EMM_SEGMENT_END) {
        uint32_t lba = get_logical_lba_for_phisical_lba(addr32);
        // char tmp[40]; sprintf(tmp, "0xD0000 W LBA: 0x%X->0x%X", addr32, lba); logMsg(tmp);
        if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
            ram_page_write(lba, value);
            return;
        }
    }
#endif
    /*if (addr32 >= 0xD0000UL && addr32 < 0xD8000UL) {
        addr32 -= 0xCC000UL; // TODO: why?
    }*/
#if SD_CARD_SWAP
    else if ((addr32) > 0xFFFFFUL) {
        if (addr32 >= 0x100000UL && addr32 <= 0xFFFF0UL + 0xFFFFUL) { // Hihg mem (1Mb + 64Kb)
          if (get_a20_enabled()) { // A20 line is ON
            // char tmp[40]; sprintf(tmp, "HIMEM W LBA: 0x%X", addr32); logMsg(tmp);
            ram_page_write(addr32, value);
            return;
          }
        }
        write86(addr32 - 0x100000UL, value);
        return;
#endif
#if PICO_ON_DEVICE
    } else if (PSRAM_AVAILABLE) {
        psram_write8(&psram_spi, addr32, value);
    }
#if SD_CARD_SWAP
    else {
        ram_page_write(addr32, value);
    }
#endif
#endif
}

void writew86(uint32_t addr32, uint16_t value) {
#if PICO_ON_DEVICE
    if (PSRAM_AVAILABLE && (addr32 > (RAM_SIZE << 10) && addr32 < (640 << 10))) {
        psram_write16(&psram_spi, addr32, value);
    }
    else
#if SD_CARD_SWAP
    if (addr32 >= RAM_PAGE_SIZE && addr32 < (640 << 10) - 1 && (addr32 & 0xFFFFFFFE) == 0) {
        ram_page_write16(addr32, value);
    } else // TODO: ROM, VRAM, himiem, ems...
#endif
#endif
    {
        write86(addr32, (uint8_t)value);
        write86(addr32 + 1, (uint8_t)(value >> 8));
    }
}

__inline uint8_t read86(uint32_t addr32) {
#if PICO_ON_DEVICE && SD_CARD_SWAP
    if ((!PSRAM_AVAILABLE && addr32 < (640 << 10)) || PSRAM_AVAILABLE && addr32 < (RAM_SIZE << 10)) {
#else
    if (addr32 < (RAM_SIZE << 10)) {
#endif
        // https://docs.huihoo.com/gnu_linux/own_os/appendix-bios_memory_2.htm
#if SD_CARD_SWAP
                if (PSRAM_AVAILABLE || addr32 < 4096) {
                    // do not touch first 4kb
                    return RAM[addr32];
                }
                return ram_page_read(addr32);
#else
        return RAM[addr32];
#endif
    }
    else if (addr32 == 0xFC000) {
        // TANDY graphics hack
        return 0x21;
    }
    else if ((addr32 >= 0xFE000UL) && (addr32 <= 0xFFFFFUL)) {
        //if (addr32 == 0xFFFFEUL) { // TODO: rebuild BIOS to change this byte (in other case internal BIOS test failed)
        //    return 0xFD; // PCjr
        //}
        // BIOS ROM range
        addr32 -= 0xFE000UL;
        return BIOS[addr32];
    }
#if SD_CARD_SWAP
    if ((addr32 >> 4) >= PHISICAL_EMM_SEGMENT && (addr32 << 4) < PHISICAL_EMM_SEGMENT_END) {
        uint32_t lba = get_logical_lba_for_phisical_lba(addr32);
        // char tmp[40]; sprintf(tmp, "0xD0000 W LBA: 0x%X->0x%X", addr32, lba); logMsg(tmp);
        if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
            return ram_page_read(lba);
        }
    }
#endif
    if ((addr32 >= 0xB8000UL) && (addr32 < 0xC0000UL)) {
        // video RAM range
        addr32 -= 0xB8000UL;
        return VRAM[addr32]; //
    }
    if ((addr32 >= 0xD0000UL) && (addr32 < 0xD8000UL)) { // Expanded memory paging space D00000-E00000
        // NE2000
        addr32 -= 0xCC000UL; // TODO: why?
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

    else if (addr32 >= 0x100000UL && addr32 <= 0xFFFF0UL + 0xFFFFUL) { // Hihg mem (1Mb + 64Kb)
#if SD_CARD_SWAP
// TODO: PSRAM_AVAILABLE ...
        if (get_a20_enabled()) {
            // char tmp[40]; sprintf(tmp, "HIMEM LBA: 0x%X", addr32); logMsg(tmp);
            return ram_page_read(addr32);
        }
        return read86(addr32 - 0x100000UL); // FFFF:0010 -> 0000:0000 rolling address space for case A20 is turned off
#endif
    }
#if PICO_ON_DEVICE
    if (PSRAM_AVAILABLE) {
        return psram_read8(&psram_spi, addr32);
    }
#endif
    return 0;
}

uint16_t readw86(uint32_t addr32) {
#if PICO_ON_DEVICE
    if (PSRAM_AVAILABLE && (addr32 > (RAM_SIZE << 10) && addr32 < (640 << 10))) {
        return psram_read16(&psram_spi, addr32);
    }
#if SD_CARD_SWAP
    auto w = (addr32 & 0xFFFFFFFE) == 0;
    if (addr32 >= RAM_PAGE_SIZE && addr32 < (640 << 10) - 1 && w) {
        return ram_page_read16(addr32);
    } // TODO: ROM, VRAM, ...
    if (get_a20_enabled() && addr32 >= 0x100000UL && addr32 <= 0xFFFF0UL + 0xFFFFUL && w) { // Hihg mem (1Mb + 64Kb)
        return ram_page_read16(addr32);
    }
#endif
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
uint32_t ClockTick(uint32_t interval, void* name) {
    doirq(0);
    tickssource();
    return timer_period / 1000;
}

uint32_t BlinkTimer(uint32_t interval, void* name) {
    cursor_blink_state ^= 1;
    return interval;
}
#endif

void reset86() {
#if !PICO_ON_DEVICE
    SDL_AddTimer(timer_period / 1000, ClockTick, "timer");
    SDL_AddTimer(500, BlinkTimer, "blink");
#endif
    init_emm();
    init8253();
    init8259();
    initsermouse(0x378, 4);

    memset(RAM, 0x0, RAM_SIZE << 10);
    memset(VRAM, 0x0, VRAM_SIZE << 10);
#if SD_CARD_SWAP
    gpio_put(PICO_DEFAULT_LED_PIN, true);
    for (size_t page = 0; page < RAM_BLOCKS; ++page) {
        RAM_PAGES[page] = page;
    }
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

static void custom_on_board_emm() {
    char tmp[80];
    uint16_t FN = CPU_AH;
    // sprintf(tmp, "LIM40 FN %Xh", FN); logMsg(tmp);
    switch(CPU_AH) { // EMM LIM 4.0
    // The Get Status function returns a status code indicating whether the
    // memory manager is present and the hardware is working correctly.
    case 0x40: {
        CPU_AX = 0;
        zf = 0;
        return;
    }
    // The Get Page Frame Address function returns the segment address where
    // the page frame is located.
    case 0x41: {
        CPU_BX = emm_conventional_segment(); // page frame segment address
        // sprintf(tmp, "LIM40 FN %Xh -> 0x%X (page frame segment)", FN, CPU_BX); logMsg(tmp);
        CPU_AX = 0;
        zf = 0;
        return;
    }
    // The Get Unallocated Page Count function returns the number of
    // unallocated pages and the total number of expanded memory pages.
    case 0x42: {
        CPU_BX = unallocated_emm_pages();
        CPU_DX = total_emm_pages();
        // sprintf(tmp, "LIM40 FN %Xh -> %d free of %d EMM pages", FN, CPU_BX, CPU_DX); logMsg(tmp);
        CPU_AX = 0;
        zf = 0;
        return;
    }
    // The Allocate Pages function allocates the number of pages requested
    // and assigns a unique EMM handle to these pages. The EMM handle owns
    // these pages until the application deallocates them.
    case 0x43: {
        CPU_DX = allocate_emm_pages(CPU_BX, &CPU_AX);
        // sprintf(tmp, "LIM40 FN %Xh err: %Xh alloc(%d pages); handler: %d", FN, CPU_AH, CPU_BX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Map/Unmap Handle Page function maps a logical page at a specific
    // physical page anywhere in the mappable regions of system memory.
    case 0x44: {
        // AL = physical_page_number
        // BX = logical_page_number, if FFFFh, the physical page specified in AL will be unmapped
        // DX = emm_handle
        auto AL = CPU_AL;
        CPU_AX = map_unmap_emm_page(CPU_AL, CPU_BX, CPU_DX);
        sprintf(tmp, "LIM40 FN %Xh res: phisical page %Xh was mapped to %Xh logical one for 0x%X EMM handler",
                      FN, AL, CPU_AX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // Deallocate Pages deallocates the logical pages currently allocated to an EMM handle.
    case 0x45: {
        auto emm_handle = CPU_DX;
        CPU_AX = deallocate_emm_pages(emm_handle);
        // sprintf(tmp, "LIM40 FN %Xh res: %Xh - EMM handler dealloc", FN, emm_handle); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Get Version function returns the version number of the memory manager software.
    case 0x46: {
        /*
                                     0100 0000
                                       /   \
                                      4  .  0
        */
        CPU_AL = 0b01000000; // 4.0
        CPU_AH = 0; zf = 0;
        return;
    }
    // Save Page Map saves the contents of the page mapping registers on all
    // expanded memory boards in an internal save area.
    case 0x47: {
        CPU_AX = save_emm_mapping(CPU_DX);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh (save mapping for %Xh)", FN, CPU_AX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Restore Page Map function restores the page mapping register
    // contents on the expanded memory boards for a particular EMM handle.
    // This function lets your program restore the contents of the mapping
    // registers its EMM handle saved.
    case 0x48: {
        logMsg("Restore Page Map - not implemented - nothing to do");
        CPU_AX = restore_emm_mapping(CPU_DX);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh (restore mapping for %Xh)", FN, CPU_AX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Get Handle Count function returns the number of open EMM handles
    // (including the operating system handle 0) in the system.
    case 0x4B: {
        CPU_BX = total_open_emm_handles();
        sprintf(tmp, "LIM40 FN %Xh res: %Xh - total_emm_handlers", FN, CPU_BX); logMsg(tmp);
        CPU_AX = 0; zf = 0;
        return;
    }
    // The Get Handle Pages function returns the number of pages allocated to
    // a specific EMM handle.
    case 0x4C: {
        CPU_BX = get_emm_handle_pages(CPU_DX, &CPU_AX);
        sprintf(tmp, "LIM40 FN %Xh handler: %d; res: %Xh; pages handled: %d", FN, CPU_DX, CPU_AX, CPU_BX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Get All Handle Pages function returns an array of the open emm
    // handles and the number of pages allocated to each one.
    case 0x4D: {
        // ES:DI = pointer to handle_page
        uint32_t addr32 = ((uint32_t)CPU_ES << 4) + CPU_DI;
        CPU_BX = get_all_emm_handle_pages(addr32);
        // sprintf(tmp, "LIM40 FN %Xh all_handlers: %Xh (pages)", FN, CPU_BX); logMsg(tmp);
        CPU_AX = 0; zf = 0;
        return;
    }
    case 0x4E:
        FN = CPU_AX;
        switch(CPU_AL) {
        // The Get Page Map subfunction saves the mapping context for all
        // mappable memory regions (conventional and expanded) by copying the
        // contents of the mapping registers from each expanded memory board to a
        // destination array.
        case 0x00: {
            // ES:DI = dest_page_map
            uint32_t addr32 = ((uint32_t)CPU_ES << 4) + CPU_DI;
            get_emm_pages_map(addr32);
            sprintf(tmp, "LIM40 FN %Xh emm pages: %Xh done", FN, CPU_AX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // The Set Page Map subfunction restores the mapping context for all
        // mappable memory regions (conventional and expanded) by copying the
        // contents of a source array into the mapping registers on each expanded
        // memory board in the system.
        case 0x01: {
            // DS:SI = source_page_map
            uint32_t addr32 = ((uint32_t)CPU_DS << 4) + CPU_SI;
            set_emm_pages_map(addr32);
            sprintf(tmp, "LIM40 FN %Xh (save emm) done", FN); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // The Get & Set subfunction simultaneously saves a current mapping
        // context and restores a previous mapping context for all mappable
        // memory regions (both conventional and expanded).
        case 0x02: {
            // ES:DI = dest_page_map
            // DS:SI = source_page_map
            get_emm_pages_map(((uint32_t)CPU_ES << 4) + CPU_DI);
            set_emm_pages_map(((uint32_t)CPU_DS << 4) + CPU_SI);
            sprintf(tmp, "LIM40 FN %Xh (get/save) done", FN); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // The Get Size of Page Map Save Array subfunction returns the storage
        // requirements for the array passed by the other three subfunctions.
        // This subfunction doesn't require an EMM handle.
        case 0x03: {
            CPU_AX = get_emm_pages_map_size();
            sprintf(tmp, "LIM40 FN %Xh emm pages size: %Xh", FN, CPU_AX); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    case 0x4F:
        FN = CPU_AX;
        switch(CPU_AL) {
        // The Get Partial Page Map subfunction saves a partial mapping context
        // for specific mappable memory regions in a system.
        case 0x00: {
            // DS:SI = partial_page_map
            // ES:DI = dest_array
            CPU_AX = get_partial_emm_page_map((CPU_DS << 4) + CPU_SI, (CPU_ES << 4) + CPU_DI);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_partial_emm_page_map (w/a)", FN, CPU_AH); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        case 0x01: {
            // DS:SI = source_array
            CPU_AX = set_partial_emm_page_map((CPU_DS << 4) + CPU_SI);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh set_partial_emm_page_map (w/a)", FN, CPU_AH); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // The Return Size subfunction returns the storage requirements for the
        // array passed by the other two subfunctions.  This subfunction doesn't
        // require an EMM handle.
        case 0x03: {
            // BX = number of pages in the partial array
            CPU_AX = get_emm_pages_map_size(); // W/A
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_emm_pages_map_size (w/a)", FN, CPU_AX); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    case 0x50:
        FN = CPU_AX;
        switch(CPU_AL) {
        // MAPPING AND UNMAPPING MULTIPLE PAGES
        case 0x00: {
            uint16_t handle = CPU_DX;
            uint16_t log_to_phys_map_len = CPU_CX;
            uint32_t log_to_phys_map = ((uint32_t)CPU_DS << 4) + CPU_SI;
            CPU_AX = map_unmap_emm_pages(handle, log_to_phys_map_len, log_to_phys_map);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh map_unmap_emm_pages(%d, %d, %Xh)",
                    FN, CPU_AX, handle, log_to_phys_map_len, log_to_phys_map); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // MAP/UNMAP MULTIPLE HANDLE PAGES (by physical segments)
        case 0x01: {
            uint16_t handle = CPU_DX;
            uint16_t log_to_segment_map_len = CPU_CX;
            uint32_t log_to_segment_map = ((uint32_t)CPU_DS << 4) + CPU_SI;
            CPU_AX = map_unmap_emm_seg_pages(handle, log_to_segment_map_len, log_to_segment_map);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh map_unmap_emm_seg_pages(%d, %d, %Xh)",
                    FN, CPU_AX, handle, log_to_segment_map_len, log_to_segment_map); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    // REALLOCATE PAGES
    case 0x51: {
        CPU_AX = reallocate_emm_pages(CPU_DX, CPU_BX);
        // sprintf(tmp, "LIM40 FN %Xh err: %Xh realloc(%d pages); handler: %d", FN, CPU_AH, CPU_BX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // Optional: set handler attributes
    case 0x52: {
        CPU_AX = 0x9100; // not supported
        zf = 1;
        return;
    }
    case 0x53:
        FN = CPU_AX;
        switch(CPU_AL) {
        // GET HANDLE NAME 
        case 0x00: {
            uint16_t handle = CPU_DX;
            uint32_t handle_name = ((uint32_t)CPU_ES << 4) + CPU_DI;
            CPU_AX = get_handle_name(handle, handle_name);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_handle_name(%d, %Xh)",
                    FN, CPU_AX, handle, handle_name); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // SET HANDLE NAME
        case 0x01: {
            uint16_t handle = CPU_DX;
            uint32_t handle_name = ((uint32_t)CPU_DS << 4) + CPU_SI;
            CPU_AX = set_handle_name(handle, handle_name);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_handle_name(%d, %Xh)",
                    FN, CPU_AX, handle, handle_name); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    case 0x54:
        FN = CPU_AX;
        switch(CPU_AL) {
        // GET HANDLE DIRECTORY
        case 0x00: {
            uint32_t handle_dir_struct = ((uint32_t)CPU_ES << 4) + CPU_DI;
            CPU_AX = get_handle_dir(handle_dir_struct);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh handle_dir_struct(%Xh)", FN, CPU_AX, handle_dir_struct); logMsg(tmp);
            if (CPU_AH) zf = 1; else zf = 0;
            return;
        }
        // SEARCH FOR NAMED HANDLE
        case 0x01: {
            uint32_t handle_name = ((uint32_t)CPU_ES << 4) + CPU_DI;
            CPU_DX = lookup_handle_dir(handle_name , &CPU_AX);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh handle: %d lookup_handle_dir(%Xh)", FN, CPU_AH, CPU_DX, handle_name); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // GET TOTAL HANDLES
        case 0x02: {
            CPU_BX = MAX_EMM_HANDLERS;
            // sprintf(tmp, "LIM40 FN %Xh MAX_EMM_HANDLERS: %d", FN, CPU_BX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // TODO: 
    }
    // ALTER PAGE MAP & JUMP
    case 0x55: {
        uint8_t page_number_segment_selector = CPU_AL;
        uint16_t handle = CPU_DX;
        uint32_t map_and_jump = ((uint32_t)CPU_DS << 4) + CPU_SI;
        CPU_AH = map_emm_and_jump(page_number_segment_selector, handle, map_and_jump);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh handle: %d page_number_segment_selector: %d (not implemented)",
                      FN, CPU_AH, CPU_DX, page_number_segment_selector); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    // ALTER PAGE MAP & CALL
    case 0x56: {
        uint8_t page_number_segment_selector = CPU_AL;
        uint16_t handle = CPU_DX;
        uint32_t map_and_call = ((uint32_t)CPU_DS << 4) + CPU_SI;
        CPU_AH = map_emm_and_call(page_number_segment_selector, handle, map_and_call);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh handle: %d page_number_segment_selector: %d (not implemented)",
                      FN, CPU_AH, CPU_DX, page_number_segment_selector); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    // MOVE/EXCHANGE MEMORY REGION
    case 0x57: {
        CPU_AH = 0x86;
        sprintf(tmp, "LIM40 FN %Xh MOVE/EXCHANGE MEMORY REGION (not implemented)", FN); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    case 0x58:
        FN = CPU_AX;
        switch(CPU_AL) {
        // GET MAPPABLE PHYSICAL ADDRESS ARRAY
        case 0x00: {
            uint32_t mappable_phys_page = ((uint32_t)CPU_ES << 4) + CPU_DI;
            CPU_AX = get_mappable_physical_array(mappable_phys_page);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_mappable_physical_array(%Xh)",
                    FN, CPU_AX, mappable_phys_page); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // GET MAPPABLE PHYSICAL ADDRESS ARRAY ENTRIES
        case 0x01: {
            CPU_CX = get_mappable_phys_pages();
            sprintf(tmp, "LIM40 FN %Xh get_mappable_phys_pages: %d", FN, CPU_CX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
    }
    case 0x59:
        FN = CPU_AX;
        switch(CPU_AL) {
        // GET HARDWARE CONFIGURATION ARRAY
        case 0x00: {
            uint32_t hardware_info = ((uint32_t)CPU_ES << 4) + CPU_DI;
            get_hardvare_emm_info(hardware_info);
            sprintf(tmp, "LIM40 FN %Xh %d of %d free pages", FN, CPU_BX, CPU_DX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // GET UNALLOCATED RAW PAGE COUNT
        case 0x01: {
            CPU_BX = unallocated_emm_pages();
            CPU_DX = total_emm_pages();
            sprintf(tmp, "LIM40 FN %Xh %d of %d free pages", FN, CPU_BX, CPU_DX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
    }
    case 0x5A:
        FN = CPU_AX;
        switch(CPU_AL) {
        //  ALLOCATE STANDARD PAGES
        case 0x00: {
            CPU_AX = allocate_emm_pages_sys(CPU_BX, CPU_DX);
            sprintf(tmp, "LIM40 FN %Xh (%d, %d) allocate_emm_pages_sys res: %Xh", FN, CPU_BX, CPU_DX, CPU_AX); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        //  ALLOCATE STANDARD/RAW
        case 0x01: {
            CPU_AX = allocate_emm_raw_pages(CPU_BX);
            sprintf(tmp, "LIM40 FN %Xh (%d, %d) allocate_emm_raw_pages res: %Xh (not yet)", FN, CPU_BX, CPU_DX, CPU_AX); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    case 0x5B: {
        CPU_AH = 0x86;
        sprintf(tmp, "LIM40 FN %Xh ALTERNATE MAP REGISTER SET (not implemented)", FN); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    case 0x5C: {
        CPU_AH = 0x86;
        sprintf(tmp, "LIM40 FN %Xh PREPARE EXPANDED MEMORY HARDWARE FOR WARM BOOT (not implemented)", FN); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    case 0x5D: {
        CPU_AH = 0x86;
        sprintf(tmp, "LIM40 FN %Xh ENABLE/DISABLE OS/E FUNCTION SET (not implemented)", FN); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    default:
        sprintf(tmp, "LIM40 FN %Xh (not implemented)", CPU_AX); logMsg(tmp);
    }
}

void intcall86(uint8_t intnum) {
    switch (intnum) {
        case 0x67: {
            custom_on_board_emm();
            return;
        }
        case 0x15:
            switch(CPU_AH) {
                case 0x24: 
                    switch(CPU_AL) {
                        case 0x00:
                            set_a20(1);
                            cf = 0; CPU_AH = 0;
                            logMsg("INT15! 2400 |A20_ENABLE_BIT");
                            return;
                        case 0x01:
                            set_a20(0);
                            cf = 0; CPU_AH = 0;
                            logMsg("INT15! 2401 ~A20_ENABLE_BIT");
                            return;
                        case 0x02:
                            CPU_AL = get_a20_enabled();
                            cf = 0; CPU_AH = 0;{
                                char tmp[80]; sprintf(tmp, "INT15! 2402 AL: 0x%X (A20 line)", CPU_AL); logMsg(tmp);
                            }
                            return;
                        case 0x03:
                            CPU_BX = 3;
                            CPU_AH = 0;
                            cf = 0;
                            logMsg("INT15! 2403 BX: 3");
                            return;
                    }
                    break;
       /*       case 0x4F:
                    CPU_AH = 0x86;
                    cf = 1;
                    return;
                case 0x52: // removable media eject
                    // TODO:
                    CPU_AH = 0;
                    cf = 0;
                    return;
                case 0x53: // APM
                    // TODO:
                    break;
                case 0x5F: // VGA hooks
                case 0x7F:
                    // TODO:
                    break;
                case 0x83: // real-time clock
                case 0x86:
                    // TODO:
                    break;*/
                case 0x87: {
                    // +++ should probably have descriptor checks
                    // +++ should have exception handlers
                    uint8_t prev_a20_enable = set_a20(1); // enable A20 line
                    // 128K max of transfer on 386+ ???
                    // source == destination ???

                    // ES:SI points to descriptor table
                    // offset   use     initially  comments
                    // ==============================================
                    // 00..07   Unused  zeros      Null descriptor
                    // 08..0f   GDT     zeros      filled in by BIOS
                    // 10..17   source  ssssssss   source of data
                    // 18..1f   dest    dddddddd   destination of data
                    // 20..27   CS      zeros      filled in by BIOS
                    // 28..2f   SS      zeros      filled in by BIOS

                    // check for access rights of source & dest here

                    // Initialize GDT descriptor
                    /* TODO: translate to arm
    u64 *gdt_far = (void*)(regs->si + 0);
    u16 gdt_seg = regs->es;
    u32 loc = (u32)MAKE_FLATPTR(gdt_seg, gdt_far);
    SET_FARVAR(gdt_seg, gdt_far[1], GDT_DATA | GDT_LIMIT((6*sizeof(u64))-1)
               | GDT_BASE(loc));
    // Initialize CS descriptor
    u64 lim = GDT_LIMIT(0x0ffff);
    if (in_post())
        lim = GDT_GRANLIMIT(0xffffffff);
    SET_FARVAR(gdt_seg, gdt_far[4], GDT_CODE | lim | GDT_BASE(BUILD_BIOS_ADDR));
    // Initialize SS descriptor
    loc = (u32)MAKE_FLATPTR(GET_SEG(SS), 0);
    SET_FARVAR(gdt_seg, gdt_far[5], GDT_DATA | lim | GDT_BASE(loc));

    SET_SEG(ES, gdt_seg);
    u16 count = regs->cx, si = 0, di = 0;
    asm volatile(
        // Load new descriptor tables
        "  lgdtw %%es:(1<<3)(%%eax)\n"
        "  lidtw %%cs:pmode_IDT_info\n"

        // Enable protected mode
        "  movl %%cr0, %%eax\n"
        "  orl $" __stringify(CR0_PE) ", %%eax\n"
        "  movl %%eax, %%cr0\n"

        // far jump to flush CPU queue after transition to protected mode
        "  ljmpw $(4<<3), $1f\n"

        // GDT points to valid descriptor table, now load DS, ES
        "1:movw $(2<<3), %%ax\n" // 2nd descriptor in table, TI=GDT, RPL=00
        "  movw %%ax, %%ds\n"
        "  movw $(3<<3), %%ax\n" // 3rd descriptor in table, TI=GDT, RPL=00
        "  movw %%ax, %%es\n"

        // memcpy CX words using 32bit memcpy if applicable
        "  testw $1, %%cx\n"
        "  jnz 3f\n"
        "  shrw $1, %%cx\n"
        "  rep movsl %%ds:(%%si), %%es:(%%di)\n"

        // Restore DS and ES segment limits to 0xffff
        "2:movw $(5<<3), %%ax\n" // 5th descriptor in table (SS)
        "  movw %%ax, %%ds\n"
        "  movw %%ax, %%es\n"

        // Disable protected mode
        "  movl %%cr0, %%eax\n"
        "  andl $~" __stringify(CR0_PE) ", %%eax\n"
        "  movl %%eax, %%cr0\n"

        // far jump to flush CPU queue after transition to real mode
        "  ljmpw $" __stringify(SEG_BIOS) ", $4f\n"

        // Slower 16bit copy method
        "3:rep movsw %%ds:(%%si), %%es:(%%di)\n"
        "  jmp 2b\n"

        // restore IDT to normal real-mode defaults
        "4:lidtw %%cs:rmode_IDT_info\n"

        // Restore %ds (from %ss)
        "  movw %%ss, %%ax\n"
        "  movw %%ax, %%ds\n"
        : "+a" (gdt_far), "+c"(count), "+m" (__segment_ES)
        : "S" (si), "D" (di)
        : "cc");

    set_a20(prev_a20_enable);

    set_code_success(regs);*/
                    }
                    break;
                case 0x88:
                    if (ON_BOARD_RAM_KB > 64 * 1024) {
                        CPU_AX = 63 * 1024;
                    } else {
                        CPU_AX = ON_BOARD_RAM_KB - 1024;
                    }
                    cf = 0;
                    return;
     /*           case 0x89: {
                       char tmp[80]; sprintf(tmp, "INT15- 89 AX: 0x%X (Switch to protected mode)", CPU_AX); logMsg(tmp);
                    }
                    CPU_AH = 0x86;
                    cf = 1;// switch to protected mode 286+
                    return;
                case 0x90: // Device busy interrupt.  Called by Int 16h when no key available
                    break;
                case 0x91: // Interrupt complete.  Called by Int 16h when key becomes available
                    break;
                case 0xC0: // to be processed by BIOS
                    // CPU_ES = 0xF000; // BIOS segment
                    // CPU_BX = 0xE6f5; // BIOS config table
                    break;*/
                case 0xE8:
                    switch(CPU_AL) {
                        case 0x01:
                            if (ON_BOARD_RAM_KB > 16*1024) {
                                CPU_CX = 1024 * 15; // 15MB
                                CPU_DX = (ON_BOARD_RAM_KB - 16 * 1024) / 64;
                            } else {
                                CPU_CX = ON_BOARD_RAM_KB - 1;
                                CPU_DX = 0;
                            }
                            CPU_AX = CPU_CX;
                            CPU_BX = CPU_DX;
                            cf = 0;
                            return;
                        case 0x20: {
                                // ES:DI - destination for the table
                                int count = e820_count;
                                if (CPU_DX != 0x534D4150 || CPU_BX >= count || CPU_CX < sizeof(e820_list[0])) {
                                    CPU_AH = 0x86;
                                    cf = 1;
                                    logMsg("INT15! E820 failed");
                                    return;
                                }
                                memcpy_far(CPU_ES,
                                           (void*)(CPU_DI + 0),
                                           get_global_seg(),
                                           &e820_list[CPU_BX],
                                           sizeof(e820_list[0])
                                );
                                if (CPU_BX == count - 1)
                                    CPU_BX = 0;
                                else
                                    CPU_BX++;
                                CPU_AX = 0x534D4150;
                                CPU_CX = sizeof(e820_list[0]);
                                // char tmp[80]; sprintf(tmp, "INT15! E820 CX: 0x%X; BX: 0x%X", CPU_CX, CPU_BX); logMsg(tmp);
                                cf = 0;
                                return;
                            }
                        default:
                            break;
                    }
                    break;
                default: {
                    // char tmp[80]; sprintf(tmp, "INT 15h CPU_AH: 0x%X; CPU_AL: 0x%X", CPU_AH, CPU_AL); logMsg(tmp);
                }
            }
            break;
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
#if PICO_ON_DEVICE
            insertdisk(0, fdd0_sz(), fdd0_rom(), "\\XT\\fdd0.img");
            insertdisk(1, fdd1_sz(), fdd1_rom(), "\\XT\\fdd1.img");
            insertdisk(128, 0, NULL, "\\XT\\hdd.img");
            keyboard_send(0xFF);
#else
            //insertdisk(0, sizeof FDD0, FDD0, NULL);
            if (1 == insertdisk(0, 0, NULL, "fdd0.img") ) {
                insertdisk(0, sizeof FDD0, FDD0, NULL);
            }
            insertdisk(1, sizeof FDD1, FDD1, NULL);
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
            return 0x01; // escape
        case 0x30:
            return 0x0B; // zero
        case 0x31: // numeric keys 1-9
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
            return 0x3B; // F1
        case SDLK_F2:
            return 0x3C; // F2
        case SDLK_F3:
            return 0x3D; // F3
        case SDLK_F4:
            return 0x3E; // F4
        case SDLK_F5:
            return 0x3F; // F5
        case SDLK_F6:
            return 0x40; // F6
        case SDLK_F7:
            return 0x41; // F7
        case SDLK_F8:
            return 0x42; // F8
        case SDLK_F9:
            return 0x43; // F9
        case SDLK_F10:
            return 0x44; // F10
        case SDLK_NUMLOCKCLEAR:
            return 0x45; // numlock
        case SDLK_SCROLLLOCK:
            return 0x46; // scroll lock
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
            return -1; // *** UNSUPPORTED KEY ***
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
                }
                else if (!hijacked_input)
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
