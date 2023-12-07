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

#define CPU_V20

#if PICO_ON_DEVICE

#include "pico/time.h"
#include "ps2.h"
#include "vga.h"
#include "psram_spi.h"

#else

#include "SDL2/SDL.h"


// PSRAM and pages mock
uint8_t EXTRAM[EXT_RAM_SIZE];
void * psram_spi;
#define psram_read8(a, addr32) EXTRAM[addr32]
#define psram_read16(a, addr32) (uint16_t)EXTRAM[addr32]


#define psram_write8(a, addr32, value) EXTRAM[addr32] = value
#define psram_write16(a, addr32, value) EXTRAM[addr32] = value & 0xFF; \
EXTRAM[addr32] = value >> 8;

#define psram_write32(a, addr32, value)


uint8_t ram_page_read(uint32_t addr32) {
    return EXTRAM[addr32];
}

uint8_t read8psram(uint32_t addr32) {
    return EXTRAM[addr32];
}

uint16_t ram_page_read16(uint32_t addr32) {
    return (EXTRAM[addr32] + EXTRAM[addr32+1] << 8);
}

uint16_t read16psram(uint32_t addr32) {
    return (EXTRAM[addr32] + EXTRAM[addr32+1] << 8);
}

void ram_page_write(uint32_t addr32, uint8_t value) {
    EXTRAM[addr32] = value;
}

void write8psram(uint32_t addr32, uint8_t value) {
    EXTRAM[addr32] = value;
}
void ram_page_write16(uint32_t addr32, uint16_t value) {
    EXTRAM[addr32] = value & 0xFF;
    EXTRAM[addr32] = value >> 8;
}
void write16psram(uint32_t addr32, uint16_t value) {
    EXTRAM[addr32] = value & 0xFF;
    EXTRAM[addr32] = value >> 8;
}


#define init_vram() 1
#define psram_cleanup() 1

#endif

static bool a20_line_open = false;

void notify_a20_line_state_changed(bool v) {
    //if (v) logMsg("A20 ON");
    //else logMsg("A20 OFF'");
    a20_line_open = v;
}

bool is_a20_line_open() {
    return a20_line_open;
}

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
uint8_t RAM[RAM_SIZE];
#if PICO_ON_DEVICE
__aligned(4096)
#endif
uint8_t VIDEORAM[VIDEORAM_SIZE];

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

#ifdef HANDLE_REBOOT
static bool reboot_exec = false;
void reboot_detected() {
    if (reboot_exec) {
        reboot_exec = false;
        return;
    }
    reboot_exec = true;
    logMsg("REBOOT WAS DETECTED");
    ports_reboot();
    if (!PSRAM_AVAILABLE && SD_CARD_AVAILABLE && !init_vram()) {
        logMsg((char *)"init_vram failed");
        SD_CARD_AVAILABLE = false;
        DIRECT_RAM_BORDER = PSRAM_AVAILABLE ? RAM_SIZE : (SD_CARD_AVAILABLE ? RAM_PAGE_SIZE : RAM_SIZE);
    }
    if (PSRAM_AVAILABLE) {
        psram_cleanup();
    }
    init_cpu_addresses_map();

    if (!PSRAM_AVAILABLE && !SD_CARD_AVAILABLE) {
        logMsg((char *)"Mo PSRAM or SD CARD available. Only 150Kb RAM will be usable...");
        //sleep_ms(3000);
    }
#ifdef EMS_DRIVER
    emm_reboot();
#endif
#ifdef XMS_DRIVER
    xmm_reboot();
#endif
}
#endif

INLINE void flag_szp8(uint8_t value) {
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

INLINE void flag_szp16(uint16_t value) {
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

INLINE void flag_log8(uint8_t value) {
    flag_szp8(value);
    cf = 0;
    of = 0; /* bitwise logic ops always clear carry and overflow */
}

INLINE void flag_log16(uint16_t value) {
    flag_szp16(value);
    cf = 0;
    of = 0; /* bitwise logic ops always clear carry and overflow */
}

INLINE void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3) {
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

INLINE void flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3) {
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

INLINE void flag_add8(uint8_t v1, uint8_t v2) {
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

INLINE void flag_add16(uint16_t v1, uint16_t v2) {
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

INLINE void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3) {
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

INLINE void flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3) {
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

INLINE void flag_sub8(uint8_t v1, uint8_t v2) {
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

INLINE void flag_sub16(uint16_t v1, uint16_t v2) {
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

INLINE void op_adc8() {
    res8 = oper1b + oper2b + cf;
    flag_adc8(oper1b, oper2b, cf);
}

INLINE void op_adc16() {
    res16 = oper1 + oper2 + cf;
    flag_adc16(oper1, oper2, cf);
}

INLINE void op_add8() {
    res8 = oper1b + oper2b;
    flag_add8(oper1b, oper2b);
}

INLINE void op_add16() {
    res16 = oper1 + oper2;
    flag_add16(oper1, oper2);
}

INLINE void op_and8() {
    res8 = oper1b & oper2b;
    flag_log8(res8);
}

INLINE void op_and16() {
    res16 = oper1 & oper2;
    flag_log16(res16);
}

INLINE void op_or8() {
    res8 = oper1b | oper2b;
    flag_log8(res8);
}

INLINE void op_or16() {
    res16 = oper1 | oper2;
    flag_log16(res16);
}

INLINE void op_xor8() {
    res8 = oper1b ^ oper2b;
    flag_log8(res8);
}

INLINE void op_xor16() {
    res16 = oper1 ^ oper2;
    flag_log16(res16);
}

INLINE void op_sub8() {
    res8 = oper1b - oper2b;
    flag_sub8(oper1b, oper2b);
}

INLINE void op_sub16() {
    res16 = oper1 - oper2;
    flag_sub16(oper1, oper2);
}

INLINE void op_sbb8() {
    res8 = oper1b - (oper2b + cf);
    flag_sbb8(oper1b, oper2b, cf);
}

INLINE void op_sbb16() {
    res16 = oper1 - (oper2 + cf);
    flag_sbb16(oper1, oper2, cf);
}

INLINE void getea(uint8_t rmval) {
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

INLINE void push(uint16_t pushval) {
    CPU_SP = CPU_SP - 2;
    putmem16(CPU_SS, CPU_SP, pushval);
}

INLINE uint16_t pop() {
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
    init_cpu_addresses_map();
#if !PICO_ON_DEVICE
    SDL_AddTimer(timer_period / 1000, ClockTick, "timer");
    SDL_AddTimer(500, BlinkTimer, "blink");
#endif
#ifdef EMS_DRIVER
    init_emm();
#endif
    init8253();
    init8259();
    initsermouse(0x378, 4);

    memset(RAM, 0x0, RAM_SIZE);
    memset(VIDEORAM, 0x0, VIDEORAM_SIZE);
#if PICO_ON_DEVICE
    if (SD_CARD_AVAILABLE) {
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        for (size_t page = 0; page < RAM_BLOCKS; ++page) {
            RAM_PAGES[page] = page;
        }
    }
#endif

    CPU_CS = 0xFFFF;
    CPU_SS = 0x0000;
    CPU_SP = 0x0000;

    ip = 0x0000;

    hltstate = 0;
    videomode = 3;
}

INLINE uint16_t readrm16(uint8_t rmval) {
    if (mode < 3) {
        getea(rmval);
        return read86(ea) | ((uint16_t)read86(ea + 1) << 8);
    }
    return getreg16(rmval);
}

INLINE uint8_t readrm8(uint8_t rmval) {
    if (mode < 3) {
        getea(rmval);
        return read86(ea);
    }
    return getreg8(rmval);
}

INLINE void writerm16(uint8_t rmval, uint16_t value) {
    if (mode < 3) {
        getea(rmval);
        write86(ea, value & 0xFF);
        write86(ea + 1, value >> 8);
    }
    else {
        putreg16(rmval, value);
    }
}

INLINE void writerm8(uint8_t rmval, uint8_t value) {
    if (mode < 3) {
        getea(rmval);
        write86(ea, value);
    }
    else {
        putreg8(rmval, value);
    }
}

uint8_t tandy_hack = 0;

static void intcall86(uint8_t intnum) {
    uint32_t tempcalc, memloc, n;
    switch (intnum) {
#ifdef EMS_DRIVER
        case 0x67: {
            custom_on_board_emm();
            return;
        }
#endif
#ifdef XMS_DRIVER
        case 0x2F: {
            if (INT_2Fh()) {
                return;
            }
            break;
        }
        case 0x15:
            if (INT_15h()) {
                return;
            }
            break;
#endif
        case 0x10:
            //printf("INT 10h CPU_AH: 0x%x CPU_AL: 0x%x\r\n", CPU_AH, CPU_AL);
            switch (CPU_AH) {
                case 0x0f:
                    if (videomode < 8) break;
                    CPU_AL = videomode;
                    CPU_AH = 80;
                    CPU_BH = 0;
                    return;
                case 0x00:
                    videomode = CPU_AL & 0x7F;
                    if (videomode == 4) {
                        port3D9 = 48;
                    }
                    else {
                        port3D9 = 0;
                    }
                //if (videomode >= 8) CPU_AL = 4;

                // FIXME!!
                    ega_plane_offset = 0;
                    RAM[0x449] = CPU_AL;
                    RAM[0x44A] = (uint8_t)videomode <= 2 ? 40 : 80;
                    RAM[0x44B] = 0;
                    RAM[0x484] = (uint8_t)(25 - 1);
                    if ((CPU_AL & 0x80) == 0x00) {
                        memset(VIDEORAM, 0x0, sizeof VIDEORAM);
                    }
                // http://www.techhelpmanual.com/114-video_modes.html
                // http://www.techhelpmanual.com/89-video_memory_layouts.html
                    char tmp[40];
                    sprintf(tmp, "VBIOS: Mode 0x%x (0x%x)", CPU_AX, videomode);
                    logMsg(tmp);
#if PICO_ON_DEVICE
                    if (videomode <= 0xd) {
                        graphics_set_buffer(VIDEORAM + 32768, 320, 200);
                    }
                    switch (videomode) {
                        case 0:
                            for (int i = 0; i < 16; i++) {
                                graphics_set_palette(i, cga_grayscale_palette[i]);
                            }
                            graphics_set_mode(TEXTMODE_40x30);
                            break;
                        case 1:
                            for (int i = 0; i < 16; i++) {
                                graphics_set_palette(i, cga_palette[i]);
                            }
                            graphics_set_mode(TEXTMODE_40x30);
                            break;


                        case 2:
                            for (int i = 0; i < 16; i++) {
                                graphics_set_palette(i, cga_grayscale_palette[i]);
                            }
                            graphics_set_mode(TEXTMODE_80x30);
                            break;
                        case 3:
                        case 7:
                        for (int i = 0; i < 16; i++) {
                            graphics_set_palette(i, cga_palette[i]);
                        }
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
                        case 0x0d:
                        case 0x0e:
                            graphics_set_buffer(VIDEORAM, 320, 200);
                        for (int i = 0; i < 256; i++) {
                            graphics_set_palette(i, vga_palette[i]);
                        }
                        graphics_set_mode(EGA_320x200x16);
                        //port3D8 = port3D8 & 0xFE;
                        break;
                        case 0x13:
                            graphics_set_buffer(VIDEORAM, 320, 200);
                            for (int i = 0; i < 256; i++) {
                                graphics_set_palette(i, vga_palette[i]);
                            }
                            graphics_set_mode(VGA_320x200x256);
                            port3D8 = port3D8 & 0xFE;
                            break;
                    }

#endif
                // Установить видеорежим
                    break;
                case 0x10: //VGA DAC functions
                    //printf("palette manipulation %x %x\r\n", CPU_AX, CPU_BX);
                if (videomode < 0x08) break;
                    switch (CPU_AL) {
                        case 0x00: {
                            // set one palette register                   EGA/VGA
                            const uint8_t b = (CPU_BH & 0b001 ? 2 : 0) + (CPU_BH & 0b111000 ? 1 : 0);
                            const uint8_t g = (CPU_BH & 0b010 ? 2 : 0) + (CPU_BH & 0b111000 ? 1 : 0);
                            const uint8_t r = (CPU_BH & 0b100 ? 2 : 0) + (CPU_BH & 0b111000 ? 1 : 0);
// TODO: Вообще нужно разные палитры?
                            if (videomode > 9) {
                                vga_palette[CPU_BL] = rgb(r * 85, g * 85, b * 85);
#if PICO_ON_DEVICE
                                graphics_set_palette(CPU_BL, vga_palette[CPU_BL]);
#endif
                            } else {
                                tandy_palette[CPU_BL] = rgb(r * 85, g * 85, b * 85);
#if PICO_ON_DEVICE
                                graphics_set_palette(CPU_BL, tandy_palette[CPU_BL]);
#endif
                            }


                            return;
                        }
                        case 0x02: // set all palette registers & border color   EGA/VGA
                            memloc = CPU_ES * 16 + CPU_DX;
                            for (n = 0; n < 17; n++) {
                                uint8_t color = read86(memloc++);
                                const uint8_t b = (color & 0b001 ? 2 : 0) + (color & 0b111000 ? 1 : 0);
                                const uint8_t g = (color & 0b010 ? 2 : 0) + (color & 0b111000 ? 1 : 0);
                                const uint8_t r = (color & 0b100 ? 2 : 0) + (color & 0b111000 ? 1 : 0);
                                vga_palette[n] = rgb(r * 85, g * 85, b * 85);
#if PICO_ON_DEVICE
                            graphics_set_palette(n, vga_palette[n]);
#endif
                            }
                            return;
                        // set one DAC color register                     VGA
                        case 0x10: //set individual DAC register
                            vga_palette[CPU_BX] = rgb((CPU_DH & 63) << 2, (CPU_CH & 63) << 2, (CPU_CL & 63) << 2);
#if PICO_ON_DEVICE
                        graphics_set_palette(CPU_BX, vga_palette[CPU_BX]);
#endif
                            return;
                        case 0x12: // set block of DAC color registers               VGA
                            memloc = CPU_ES * 16 + CPU_DX;
                            for (n = CPU_BX; n < (uint32_t)(CPU_BX + CPU_CX); n++) {
                                vga_palette[n] = rgb(read86(memloc) << 2, read86(memloc + 1) << 2,
                                                     read86(memloc + 2) << 2);
#if PICO_ON_DEVICE
                                graphics_set_palette(n, vga_palette[n]);
#endif
                                memloc += 3;
                            }
                    }
                    return;
                /*case 0x12: // VIDEO - ALTERNATE FUNCTION SELECT (PS, EGA, VGA, MCGA) - GET EGA INFO
                    CPU_BH = 0; // default BIOS setup (0=color; 1=monochrome)
                    CPU_BL = 3; //mem size code (0=64K; 1=128K; 2=192K; 3=256K)
                //(Note: if BL>4, then this is not an EGA BIOS)
                    CPU_CH = 0; //feature bits (values of those RCA connectors)
                    CPU_CL = 0; //switch settings
                    return;*/
                case 0x1A: //get display combination code (ps, vga/mcga)
                    CPU_AL = 0x1A;
                    CPU_BL = 0x08;
                    return;
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
            insertdisk(0, sizeof FDD0, FDD0, NULL);
            if (1 == insertdisk(0, 0, NULL, "fdd0.img")) {
                //insertdisk(0, fdd0_sz(), fdd0_rom(), NULL);
            }
        insertdisk(1, 0, NULL, "fdd1.img");
        //insertdisk(1, fdd1_sz(), fdd1_rom(), NULL);
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


INLINE uint8_t op_grp2_8(uint8_t cnt) {
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

INLINE uint16_t op_grp2_16(uint8_t cnt) {
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

INLINE void op_div8(uint16_t valdiv, uint8_t divisor) {
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

INLINE void op_idiv8(uint16_t valdiv, uint8_t divisor) {
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

INLINE void op_grp3_8() {
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
static uint32_t loopcount;
static uint16_t firstip;
uint8_t reg;

static inline void opcode00 (void) {
  // 00 ADD Eb Gb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getreg8 (reg);
  op_add8();
  writerm8 (rm, res8);
}
static inline void opcode01 (void) {
  // 01 ADD Ev Gv
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getreg16 (reg);
  op_add16();
  writerm16 (rm, res16);
}
static inline void opcode02 (void) {
  // 02 ADD Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  op_add8();
  putreg8 (reg, res8);
}
static inline void opcode03 (void) {
  // 03 ADD Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  op_add16();
  putreg16 (reg, res16);
}
static inline void opcode04 (void) {
  // 04 ADD CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  op_add8();
  CPU_AL = res8;
}
static inline void opcode05 (void) {
  // 05 ADD eAX Iv
  oper1 = (getreg16 (regax) );
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  op_add16();
  putreg16 (regax, res16);
}
static inline void opcode06 (void) {
  // 06 PUSH segregs[reges]
  push (segregs[reges]);
}
static inline void opcode07 (void) {
  // 07 POP segregs[reges]
  segregs[reges] = pop();
}
static inline void opcode08 (void) {
  // 08 OR Eb Gb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getreg8 (reg);
  op_or8();
  writerm8 (rm, res8);
}
static inline void opcode09 (void) {
  // 09 OR Ev Gv
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getreg16 (reg);
  op_or16();
  writerm16 (rm, res16);
}
static inline void opcode0A (void) {
  // 0A OR Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  op_or8();
  putreg8 (reg, res8);
}
static inline void opcode0B (void) {
  // 0B OR Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  op_or16();
  if ( (oper1 == 0xF802) && (oper2 == 0xF802) ) {
    sf = 0; // cheap hack to make Wolf 3D think we're a 286 so it plays
  }
  putreg16 (reg, res16);
}
static inline void opcode0C (void) {
  // 0C OR CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  op_or8();
  CPU_AL = res8;
}
static inline void opcode0D (void) {
  // 0D OR eAX Iv
  oper1 = getreg16 (regax);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  op_or16();
  putreg16 (regax, res16);
}
static inline void opcode0E (void) {
  // 0E PUSH segregs[regcs]
  push (segregs[regcs]);
}
static inline void opcode0F (void) {
  //0F POP CS
#ifndef CPU_V20
  segregs[regcs] = pop();
#endif
}
static inline void opcode10 (void) {
  // 10 ADC Eb Gb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getreg8 (reg);
  op_adc8();
  writerm8 (rm, res8);
}
static inline void opcode11 (void) {
  // 11 ADC Ev Gv
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getreg16 (reg);
  op_adc16();
  writerm16 (rm, res16);
}
static inline void opcode12 (void) {
  // 12 ADC Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  op_adc8();
  putreg8 (reg, res8);
}
static inline void opcode13 (void) {
  // 13 ADC Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  op_adc16();
  putreg16 (reg, res16);
}
static inline void opcode14 (void) {
  // 14 ADC CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  op_adc8();
  CPU_AL = res8;
}
static inline void opcode15 (void) {
  //15 ADC eAX Iv
  oper1 = getreg16 (regax);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  op_adc16();
  putreg16 (regax, res16);
}
static inline void opcode16 (void) {
  // 16 PUSH segregs[regss]
  push (segregs[regss]);
}
static inline void opcode17 (void) {
  // 17 POP segregs[regss]
  segregs[regss] = pop();
}
static inline void opcode18 (void) {
  // 18 SBB Eb Gb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getreg8 (reg);
  op_sbb8();
  writerm8 (rm, res8);
}
static inline void opcode19 (void) {
  // 19 SBB Ev Gv
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getreg16 (reg);
  op_sbb16();
  writerm16 (rm, res16);
}
static inline void opcode1A (void) {
  // 1A SBB Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  op_sbb8();
  putreg8 (reg, res8);
}
static inline void opcode1B (void) {
  // 1B SBB Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  op_sbb16();
  putreg16 (reg, res16);
}
static inline void opcode1C (void) {
  // 1C SBB CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  op_sbb8();
  CPU_AL = res8;
}
static inline void opcode1D (void) {
  // 1D SBB eAX Iv
  oper1 = getreg16 (regax);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  op_sbb16();
  putreg16 (regax, res16);
}
static inline void opcode1E (void) {
  // 1E PUSH segregs[regds]
  push (segregs[regds]);
}
static inline void opcode1F (void) {
  // 1F POP segregs[regds]
  segregs[regds] = pop();
}
static inline void opcode20 (void) {
  // 20 AND Eb Gb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getreg8 (reg);
  op_and8();
  writerm8 (rm, res8);
}
static inline void opcode21 (void) {
  // 21 AND Ev Gv
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getreg16 (reg);
  op_and16();
  writerm16 (rm, res16);
}
static inline void opcode22 (void) {
  // 22 AND Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  op_and8();
  putreg8 (reg, res8);
}
static inline void opcode23 (void) {
  // 23 AND Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  op_and16();
  putreg16 (reg, res16);
}
static inline void opcode24 (void) {
  // 24 AND CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  op_and8();
  CPU_AL = res8;
}
static inline void opcode25 (void) {
  // 25 AND eAX Iv
  oper1 = getreg16 (regax);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  op_and16();
  putreg16 (regax, res16);
}
static inline void opcode27 (void) {
  // 27 DAA
  if ( ( (CPU_AL & 0xF) > 9) || (af == 1) ) {
    oper1 = CPU_AL + 6;
    CPU_AL = oper1 & 255;
    if (oper1 & 0xFF00) {
      cf = 1;
    } else {
      cf = 0;
    }
    af = 1;
  } else {
    af = 0;
  }

  if ( ( (CPU_AL & 0xF0) > 0x90) || (cf == 1) ) {
    CPU_AL = CPU_AL + 0x60;
    cf = 1;
  } else {
    cf = 0;
  }

  CPU_AL = CPU_AL & 255;
  flag_szp8 (CPU_AL);
}
static inline void opcode28 (void) {
  // 28 SUB Eb Gb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getreg8 (reg);
  op_sub8();
  writerm8 (rm, res8);
}
static inline void opcode29 (void) {
  // 29 SUB Ev Gv
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getreg16 (reg);
  op_sub16();
  writerm16 (rm, res16);
}
static inline void opcode2A (void) {
  // 2A SUB Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  op_sub8();
  putreg8 (reg, res8);
}
static inline void opcode2B (void) {
  // 2B SUB Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  op_sub16();
  putreg16 (reg, res16);
}
static inline void opcode2C (void) {
  // 2C SUB CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  op_sub8();
  CPU_AL = res8;
}
static inline void opcode2D (void) {
  // 2D SUB eAX Iv
  oper1 = getreg16 (regax);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  op_sub16();
  putreg16 (regax, res16);
}
static inline void opcode2F (void) {
  // 2F DAS
  if ( ( (CPU_AL & 15) > 9) || (af == 1) ) {
    oper1 = CPU_AL - 6;
    CPU_AL = oper1 & 255;
    if (oper1 & 0xFF00) {
      cf = 1;
    } else {
      cf = 0;
    }
    af = 1;
  } else {
    af = 0;
  }
  if ( ( (CPU_AL & 0xF0) > 0x90) || (cf == 1) ) {
    CPU_AL = CPU_AL - 0x60;
    cf = 1;
  } else {
    cf = 0;
  }
  flag_szp8 (CPU_AL);
}
static inline void opcode30 (void) {
  // 30 XOR Eb Gb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getreg8 (reg);
  op_xor8();
  writerm8 (rm, res8);
}
static inline void opcode31 (void) {
  // 31 XOR Ev Gv
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getreg16 (reg);
  op_xor16();
  writerm16 (rm, res16);
}
static inline void opcode32 (void) {
  // 32 XOR Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  op_xor8();
  putreg8 (reg, res8);
}
static inline void opcode33 (void) {
  // 33 XOR Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  op_xor16();
  putreg16 (reg, res16);
}
static inline void opcode34 (void) {
  // 34 XOR CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  op_xor8();
  CPU_AL = res8;
}
static inline void opcode35 (void) {
  // 35 XOR eAX Iv
  oper1 = getreg16 (regax);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  op_xor16();
  putreg16 (regax, res16);
}
static inline void opcode37 (void) {
  // 37 AAA ASCII
  if ( ( (CPU_AL & 0xF) > 9) || (af == 1) ) {
    CPU_AL = CPU_AL + 6;
    CPU_AH = CPU_AH + 1;
    af = 1;
    cf = 1;
  } else {
    af = 0;
    cf = 0;
  }
  CPU_AL = CPU_AL & 0xF;
}
static inline void opcode38 (void) {
  // 38 CMP Eb Gb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getreg8 (reg);
  flag_sub8 (oper1b, oper2b);
}
static inline void opcode39 (void) {
  // 39 CMP Ev Gv
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getreg16 (reg);
  flag_sub16 (oper1, oper2);
}
static inline void opcode3A (void) {
  // 3A CMP Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  flag_sub8 (oper1b, oper2b);
}
static inline void opcode3B (void) {
  // 3B CMP Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  flag_sub16 (oper1, oper2);
}
static inline void opcode3C (void) {
  // 3C CMP CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  flag_sub8 (oper1b, oper2b);
}
static inline void opcode3D (void) {
  // 3D CMP eAX Iv
  oper1 = getreg16 (regax);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  flag_sub16 (oper1, oper2);
}
static inline void opcode3F (void) {
  // 3F AAS ASCII
  if ( ( (CPU_AL & 0xF) > 9) || (af == 1) ) {
    CPU_AL = CPU_AL - 6;
    CPU_AH = CPU_AH - 1;
    af = 1;
    cf = 1;
  } else {
    af = 0;
    cf = 0;
  }
  CPU_AL = CPU_AL & 0xF;
}
static inline void opcode40 (void) {
  // 40 INC eAX
  oldcf = cf;
  oper1 = getreg16 (regax);
  oper2 = 1;
  op_add16();
  cf = oldcf;
  putreg16 (regax, res16);
}
static inline void opcode41 (void) {
  // 41 INC eCX
  oldcf = cf;
  oper1 = getreg16 (regcx);
  oper2 = 1;
  op_add16();
  cf = oldcf;
  putreg16 (regcx, res16);
}
static inline void opcode42 (void) {
  // 42 INC eDX
  oldcf = cf;
  oper1 = getreg16 (regdx);
  oper2 = 1;
  op_add16();
  cf = oldcf;
  putreg16 (regdx, res16);
}
static inline void opcode43 (void) {
  // 43 INC eBX
  oldcf = cf;
  oper1 = getreg16 (regbx);
  oper2 = 1;
  op_add16();
  cf = oldcf;
  putreg16 (regbx, res16);
}
static inline void opcode44 (void) {
  // 44 INC eSP
  oldcf = cf;
  oper1 = getreg16 (regsp);
  oper2 = 1;
  op_add16();
  cf = oldcf;
  putreg16 (regsp, res16);
}
static inline void opcode45 (void) {
  // 45 INC eBP
  oldcf = cf;
  oper1 = getreg16 (regbp);
  oper2 = 1;
  op_add16();
  cf = oldcf;
  putreg16 (regbp, res16);
}
static inline void opcode46 (void) {
  // 46 INC eSI
  oldcf = cf;
  oper1 = getreg16 (regsi);
  oper2 = 1;
  op_add16();
  cf = oldcf;
  putreg16 (regsi, res16);
}
static inline void opcode47 (void) {
  // 47 INC eDI
  oldcf = cf;
  oper1 = getreg16 (regdi);
  oper2 = 1;
  op_add16();
  cf = oldcf;
  putreg16 (regdi, res16);
}
static inline void opcode48 (void) {
  // 48 DEC eAX *
  oldcf = cf;
  oper1 = getreg16 (regax);
  oper2 = 1;
  op_sub16();
  cf = oldcf;
  putreg16 (regax, res16);
}
static inline void opcode49 (void) {
  // 49 DEC eCX
  oldcf = cf;
  oper1 = getreg16 (regcx);
  oper2 = 1;
  op_sub16();
  cf = oldcf;
  putreg16 (regcx, res16);
}
static inline void opcode4A (void) {
  // 4A DEC eDX
  oldcf = cf;
  oper1 = getreg16 (regdx);
  oper2 = 1;
  op_sub16();
  cf = oldcf;
  putreg16 (regdx, res16);
}
static inline void opcode4B (void) {
  // 4B DEC eBX
  oldcf = cf;
  oper1 = getreg16 (regbx);
  oper2 = 1;
  op_sub16();
  cf = oldcf;
  putreg16 (regbx, res16);
}
static inline void opcode4C (void) {
  // 4C DEC eSP
  oldcf = cf;
  oper1 = getreg16 (regsp);
  oper2 = 1;
  op_sub16();
  cf = oldcf;
  putreg16 (regsp, res16);
}
static inline void opcode4D (void) {
  // 4D DEC eBP
  oldcf = cf;
  oper1 = getreg16 (regbp);
  oper2 = 1;
  op_sub16();
  cf = oldcf;
  putreg16 (regbp, res16);
}
static inline void opcode4E (void) {
  // 4E DEC eSI
  oldcf = cf;
  oper1 = getreg16 (regsi);
  oper2 = 1;
  op_sub16();
  cf = oldcf;
  putreg16 (regsi, res16);
}
static inline void opcode4F (void) {
  // 4F DEC eDI
  oldcf = cf;
  oper1 = getreg16 (regdi);
  oper2 = 1;
  op_sub16();
  cf = oldcf;
  putreg16 (regdi, res16);
}
static inline void opcode50 (void) {
  // 50 PUSH eAX
  push (getreg16 (regax) );
}
static inline void opcode51 (void) {
  // 51 PUSH eCX
  push (getreg16 (regcx) );
}
static inline void opcode52 (void) {
  // 52 PUSH eDX
  push (getreg16 (regdx) );
}
static inline void opcode53 (void) {
  // 53 PUSH eBX
  push (getreg16 (regbx) );
}
static inline void opcode54 (void) {
  // 54 PUSH eSP
  push (getreg16 (regsp) - 2);
}
static inline void opcode55 (void) {
  // 55 PUSH eBP
  push (getreg16 (regbp) );
}
static inline void opcode56 (void) {
  // 56 PUSH eSI
  push (getreg16 (regsi) );
}
static inline void opcode57 (void) {
  // 57 PUSH eDI
  push (getreg16 (regdi) );
}
static inline void opcode58 (void) {
  // 58 POP eAX
  putreg16 (regax, pop() );
}
static inline void opcode59 (void) {
  // 59 POP eCX
  putreg16 (regcx, pop() );
}
static inline void opcode5A (void) {
  // 5A POP eDX
  putreg16 (regdx, pop() );
}
static inline void opcode5B (void) {
  // 5B POP eBX
  putreg16 (regbx, pop() );
}
static inline void opcode5C (void) {
  // 5C POP eSP
  putreg16 (regsp, pop() );
}
static inline void opcode5D (void) {
  // 5D POP eBP
  putreg16 (regbp, pop() );
}
static inline void opcode5E (void) {
  // 5E POP eSI
  putreg16 (regsi, pop() );
}
static inline void opcode5F (void) {
  // 5F POP eDI
  putreg16 (regdi, pop() );
}

#ifdef CPU_V20
static inline void opcode60 (void) {
  // 60 PUSHA (80186+)
  oldsp = getreg16 (regsp);
  push (getreg16 (regax) );
  push (getreg16 (regcx) );
  push (getreg16 (regdx) );
  push (getreg16 (regbx) );
  push (oldsp);
  push (getreg16 (regbp) );
  push (getreg16 (regsi) );
  push (getreg16 (regdi) );
}
static inline void opcode61 (void) {
  // 61 POPA (80186+)
  putreg16 (regdi, pop() );
  putreg16 (regsi, pop() );
  putreg16 (regbp, pop() );
  dummy = pop();
  putreg16 (regbx, pop() );
  putreg16 (regdx, pop() );
  putreg16 (regcx, pop() );
  putreg16 (regax, pop() );
}
static inline void opcode62 (void) {
  // 62 BOUND Gv, Ev (80186+)
  modregrm();
  getea (rm);
  if (signext32 (getreg16 (reg) ) < signext32 ( getmem16 (ea >> 4, ea & 15) ) ) {
    intcall86 (5); //bounds check exception
  } else {
    ea += 2;
    if (signext32 (getreg16 (reg) ) > signext32 ( getmem16 (ea >> 4, ea & 15) ) ) {
      intcall86(5); //bounds check exception
    }
  }
}
static inline void opcode68 (void) {
  // 68 PUSH Iv (80186+)
  push (getmem16 (segregs[regcs], ip) );
  StepIP (2);
}
static inline void opcode69 (void) {
  // 69 IMUL Gv Ev Iv (80186+)
  modregrm();
  temp1 = readrm16 (rm);
  temp2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  if ( (temp1 & 0x8000L) == 0x8000L) {
    temp1 = temp1 | 0xFFFF0000L;
  }
  if ( (temp2 & 0x8000L) == 0x8000L) {
    temp2 = temp2 | 0xFFFF0000L;
  }
  temp3 = temp1 * temp2;
  putreg16 (reg, temp3 & 0xFFFFL);
  if (temp3 & 0xFFFF0000L) {
    cf = 1;
    of = 1;
  } else {
    cf = 0;
    of = 0;
  }
}
static inline void opcode6A (void) {
  // 6A PUSH Ib (80186+)
  push (getmem8 (segregs[regcs], ip) );
  StepIP (1);
}
static inline void opcode6B (void) {
  // 6B IMUL Gv Eb Ib (80186+)
  modregrm();
  temp1 = readrm16 (rm);
  temp2 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if ( (temp1 & 0x8000L) == 0x8000L) {
    temp1 = temp1 | 0xFFFF0000L;
  }
  if ( (temp2 & 0x8000L) == 0x8000L) {
    temp2 = temp2 | 0xFFFF0000L;
  }
  temp3 = temp1 * temp2;
  putreg16 (reg, temp3 & 0xFFFFL);
  if (temp3 & 0xFFFF0000L) {
    cf = 1;
    of = 1;
  } else {
    cf = 0;
    of = 0;
  }
}
static inline void opcode6C (void) {
  // 6E INSB
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }

  putmem8 (useseg, getreg16 (regsi) , portin (CPU_DX) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 1);
    putreg16 (regdi, getreg16 (regdi) - 1);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 1);
    putreg16 (regdi, getreg16 (regdi) + 1);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}
static inline void opcode6D (void) {
  // 6F INSW
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  putmem16 (useseg, getreg16 (regsi) , portin16 (CPU_DX) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 2);
    putreg16 (regdi, getreg16 (regdi) - 2);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 2);
    putreg16 (regdi, getreg16 (regdi) + 2);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}
static inline void opcode6E (void) {
  // 6E OUTSB
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  portout (CPU_DX, getmem8 (useseg, getreg16 (regsi) ) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 1);
    putreg16 (regdi, getreg16 (regdi) - 1);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 1);
    putreg16 (regdi, getreg16 (regdi) + 1);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}
static inline void opcode6F (void) {
  // 6F OUTSW
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  portout16 (CPU_DX, getmem16 (useseg, getreg16 (regsi) ) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 2);
    putreg16 (regdi, getreg16 (regdi) - 2);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 2);
    putreg16 (regdi, getreg16 (regdi) + 2);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}
#endif
static inline void opcode70 (void) {
  // 70 JO Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (of) {
    ip = ip + temp16;
  }
}
static inline void opcode71 (void) {
  // 71 JNO Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (!of) {
    ip = ip + temp16;
  }
}
static inline void opcode72 (void) {
  // 72 JB Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (cf) {
    ip = ip + temp16;
  }
}
static inline void opcode73 (void) {
  // 73 JNB Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (!cf) {
    ip = ip + temp16;
  }
}
static inline void opcode74 (void) {
  // 74 JZ Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (zf) {
    ip = ip + temp16;
  }
}
static inline void opcode75 (void) {
  // 75 JNZ Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (!zf) {
    ip = ip + temp16;
  }
}
static inline void opcode76 (void) {
  // 76 JBE Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (cf || zf) {
    ip = ip + temp16;
  }
}
static inline void opcode77 (void) {
  // 77 JA Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (!cf && !zf) {
    ip = ip + temp16;
  }
}
static inline void opcode78 (void) {
  // 78 JS Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (sf) {
    ip = ip + temp16;
  }
}
static inline void opcode79 (void) {
  // 79 JNS Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (!sf) {
    ip = ip + temp16;
  }
}
static inline void opcode7A (void) {
  // 7A JPE Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (pf) {
    ip = ip + temp16;
  }
}
static inline void opcode7B (void) {
  // 7B JPO Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (!pf) {
    ip = ip + temp16;
  }
}
static inline void opcode7C (void) {
  // 7C JL Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (sf != of) {
    ip = ip + temp16;
  }
}
static inline void opcode7D (void) {
  // 7D JGE Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (sf == of) {
    ip = ip + temp16;
  }
}
static inline void opcode7E (void) {
  // 7E JLE Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if ( (sf != of) || zf) {
    ip = ip + temp16;
  }
}
static inline void opcode7F (void) {
    // 7F JG Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (!zf && (sf == of) ) {
    ip = ip + temp16;
  }
}
static inline void opcode80 (void) {
  // 80/82 GRP1 Eb Ib
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
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
      flag_sub8 (oper1b, oper2b);
      break;
    default:
      break;  // to avoid compiler warnings
  }
  if (reg < 7) {
    writerm8 (rm, res8);
  }
}
static inline void opcode82 (void) {
  // 80/82 GRP1 Eb Ib
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
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
      flag_sub8 (oper1b, oper2b);
      break;
    default:
      break;  // to avoid compiler warnings
  }

  if (reg < 7) {
    writerm8 (rm, res8);
  }
}
static inline void opcode81 (void) {
  // 81 GRP1 Ev Iv
  modregrm();
  oper1 = readrm16 (rm);
  if (opcode == 0x81) {
    oper2 = getmem16 (segregs[regcs], ip);
    StepIP (2);
  } else {
    oper2 = signext (getmem8 (segregs[regcs], ip) );
    StepIP (1);
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
      flag_sub16 (oper1, oper2);
      break;
    default:
      break;  // to avoid compiler warnings */
  }
  if (reg < 7) {
    writerm16 (rm, res16);
  }
}
static inline void opcode83 (void) {
  // 83 GRP1 Ev Ib
  modregrm();
  oper1 = readrm16 (rm);
  if (opcode == 0x81) {
    oper2 = getmem16 (segregs[regcs], ip);
    StepIP (2);
  } else {
    oper2 = signext (getmem8 (segregs[regcs], ip) );
    StepIP (1);
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
      flag_sub16 (oper1, oper2);
      break;
    default:
      break;  // to avoid compiler warnings
  }
  if (reg < 7) {
    writerm16 (rm, res16);
  }
}
static inline void opcode84 (void) {
  // 84 TEST Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  oper2b = readrm8 (rm);
  flag_log8 (oper1b & oper2b);
}
static inline void opcode85 (void) {
  // 85 TEST Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  oper2 = readrm16 (rm);
  flag_log16 (oper1 & oper2);
}
static inline void opcode86 (void) {
  // 86 XCHG Gb Eb
  modregrm();
  oper1b = getreg8 (reg);
  putreg8 (reg, readrm8 (rm) );
  writerm8 (rm, oper1b);
}
static inline void opcode87 (void) {
  // 87 XCHG Gv Ev
  modregrm();
  oper1 = getreg16 (reg);
  putreg16 (reg, readrm16 (rm) );
  writerm16 (rm, oper1);
}
static inline void opcode88 (void) {
  // 88 MOV Eb Gb
  modregrm();
  writerm8 (rm, getreg8 (reg) );
}

static inline void opcode89 (void) {
  // 89 MOV Ev Gv
  modregrm();
  writerm16 (rm, getreg16 (reg) );
}

static inline void opcode8A (void) {
  // 8A MOV Gb Eb
  modregrm();
  putreg8 (reg, readrm8 (rm) );
}

static inline void opcode8B (void) {
  // 8B MOV Gv Ev
  modregrm();
  putreg16 (reg, readrm16 (rm) );
}

static inline void opcode8C (void) {
  // 8C MOV Ew Sw
  modregrm();
  writerm16 (rm, getsegreg (reg) );
}

static inline void opcode8D (void) {
  // 8D LEA Gv M
  modregrm();
  getea (rm);
  putreg16 (reg, ea - segbase (useseg) );
}

static inline void opcode8E (void) {
  // 8E MOV Sw Ew
  modregrm();
  putsegreg (reg, readrm16 (rm) );
}

static inline void opcode8F (void) {
  // 8F POP Ev
  modregrm();
  writerm16 (rm, pop() );
}

static inline void opcode90 (void) {
  // 90 NOP
}

static inline void opcode91 (void) {
  // 91 XCHG eCX eAX
  oper1 = getreg16 (regcx);
  putreg16 (regcx, getreg16 (regax) );
  putreg16 (regax, oper1);
}

static inline void opcode92 (void) {
  // 92 XCHG eDX eAX
  oper1 = getreg16 (regdx);
  putreg16 (regdx, getreg16 (regax) );
  putreg16 (regax, oper1);
}

static inline void opcode93 (void) {
  // 93 XCHG eBX eAX
  oper1 = getreg16 (regbx);
  putreg16 (regbx, getreg16 (regax) );
  putreg16 (regax, oper1);
}

static inline void opcode94 (void) {
  // 94 XCHG eSP eAX
  oper1 = getreg16 (regsp);
  putreg16 (regsp, getreg16 (regax) );
  putreg16 (regax, oper1);
}

static inline void opcode95 (void) {
  // 95 XCHG eBP eAX
  oper1 = getreg16 (regbp);
  putreg16 (regbp, getreg16 (regax) );
  putreg16 (regax, oper1);
}

static inline void opcode96 (void) {
  // 96 XCHG eSI eAX
  oper1 = getreg16 (regsi);
  putreg16 (regsi, getreg16 (regax) );
  putreg16 (regax, oper1);
}

static inline void opcode97 (void) {
  // 97 XCHG eDI eAX
  oper1 = getreg16 (regdi);
  putreg16 (regdi, getreg16 (regax) );
  putreg16 (regax, oper1);
}

static inline void opcode98 (void) {
  // 98 CBW
  if ( (CPU_AL & 0x80) == 0x80) {
    CPU_AH = 0xFF;
  } else {
    CPU_AH = 0;
  }
}

static inline void opcode99 (void) {
  // 99 CWD
  if ( (CPU_AH & 0x80) == 0x80) {
    putreg16 (regdx, 0xFFFF);
  } else {
    putreg16 (regdx, 0);
  }
}

static inline void opcode9A (void) {
  // 9A CALL Ap
  oper1 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  push (segregs[regcs]);
  push (ip);
  ip = oper1;
  segregs[regcs] = oper2;
}

static inline void opcode9B (void) {
  // 9B WAIT
}

static inline void opcode9C (void) {
  // 9C PUSHF
  push (makeflagsword() | 0xF800);
}

static inline void opcode9D (void) {
  // 9D POPF
  temp16 = pop();
  decodeflagsword (temp16);
}

static inline void opcode9E (void) {
  // 9E SAHF
  decodeflagsword ( (makeflagsword() & 0xFF00) | CPU_AH);
}

static inline void opcode9F (void) {
  // 9F LAHF
  CPU_AH = makeflagsword() & 0xFF;
}

static inline void opcodeA0 (void) {
  // A0 MOV CPU_AL Ob
  CPU_AL = getmem8 (useseg, getmem16 (segregs[regcs], ip) );
  StepIP (2);
}

static inline void opcodeA1 (void) {
  // A1 MOV eAX Ov
  oper1 = getmem16 (useseg, getmem16 (segregs[regcs], ip) );
  StepIP (2);
  putreg16 (regax, oper1);
}

static inline void opcodeA2 (void) {
  // A2 MOV Ob CPU_AL
  putmem8 (useseg, getmem16 (segregs[regcs], ip), CPU_AL);
  StepIP (2);
}

static inline void opcodeA3 (void) {
  // A3 MOV Ov eAX
  putmem16 (useseg, getmem16 (segregs[regcs], ip), getreg16 (regax) );
  StepIP (2);
}

static inline void opcodeA4 (void) {
  // A4 MOVSB
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  putmem8 (segregs[reges], getreg16 (regdi), getmem8 (useseg, getreg16 (regsi) ) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 1);
    putreg16 (regdi, getreg16 (regdi) - 1);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 1);
    putreg16 (regdi, getreg16 (regdi) + 1);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}

static inline void opcodeA5 (void) {
  // A5 MOVSW
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }

  putmem16 (segregs[reges], getreg16 (regdi), getmem16 (useseg, getreg16 (regsi) ) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 2);
    putreg16 (regdi, getreg16 (regdi) - 2);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 2);
    putreg16 (regdi, getreg16 (regdi) + 2);
   }

  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}

static inline void opcodeA6 (void) {
  // A6 CMPSB
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }

  oper1b = getmem8 (useseg, getreg16 (regsi) );
  oper2b = getmem8 (segregs[reges], getreg16 (regdi) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 1);
    putreg16 (regdi, getreg16 (regdi) - 1);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 1);
    putreg16 (regdi, getreg16 (regdi) + 1);
  }
  flag_sub8 (oper1b, oper2b);
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  if ( (reptype == 1) && !zf) {
    return;
  } else if ( (reptype == 2) && (zf == 1) ) {
    return;
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}

static inline void opcodeA7 (void) {
  // A7 CMPSW
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  oper1 = getmem16 (useseg, getreg16 (regsi) );
  oper2 = getmem16 (segregs[reges], getreg16 (regdi) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 2);
    putreg16 (regdi, getreg16 (regdi) - 2);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 2);
    putreg16 (regdi, getreg16 (regdi) + 2);
  }
  flag_sub16 (oper1, oper2);
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }

  if ( (reptype == 1) && !zf) {
    return;
  }

  if ( (reptype == 2) && (zf == 1) ) {
    return;
  }

  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}

static inline void opcodeA8 (void) {
  // A8 TEST CPU_AL Ib
  oper1b = CPU_AL;
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  flag_log8 (oper1b & oper2b);
}

static inline void opcodeA9 (void) {
  // A9 TEST eAX Iv
  oper1 = getreg16 (regax);
  oper2 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  flag_log16 (oper1 & oper2);
}

static inline void opcodeAA (void) {
  // AA STOSB
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  putmem8 (segregs[reges], getreg16 (regdi), CPU_AL);
  if (df) {
    putreg16 (regdi, getreg16 (regdi) - 1);
  } else {
    putreg16 (regdi, getreg16 (regdi) + 1);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}

static inline void opcodeAB (void) {
  // AB STOSW
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  putmem16 (segregs[reges], getreg16 (regdi), getreg16 (regax) );
  if (df) {
    putreg16 (regdi, getreg16 (regdi) - 2);
  } else {
    putreg16 (regdi, getreg16 (regdi) + 2);
  }

  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }

  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}

static inline void opcodeAC (void) {
  // AC LODSB
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  CPU_AL = getmem8 (useseg, getreg16 (regsi) );
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 1);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 1);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}

static inline void opcodeAD (void) {
  // AD LODSW
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  oper1 = getmem16 (useseg, getreg16 (regsi) );
  putreg16 (regax, oper1);
  if (df) {
    putreg16 (regsi, getreg16 (regsi) - 2);
  } else {
    putreg16 (regsi, getreg16 (regsi) + 2);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }

  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }

  ip = firstip;
}

static inline void opcodeAE (void) {
  // AE SCASB
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  oper1b = getmem8 (segregs[reges], getreg16 (regdi) );
  oper2b = CPU_AL;
  flag_sub8 (oper1b, oper2b);
  if (df) {
    putreg16 (regdi, getreg16 (regdi) - 1);
  } else {
    putreg16 (regdi, getreg16 (regdi) + 1);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  if ( (reptype == 1) && !zf) {
    return;
  } else if ( (reptype == 2) && (zf == 1) ) {
    return;
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
      return;
  }
  ip = firstip;
}

static inline void opcodeAF (void) {
  // AF SCASW
  if (reptype && (getreg16 (regcx) == 0) ) {
    return;
  }
  oper1 = getmem16 (segregs[reges], getreg16 (regdi) );
  oper2 = getreg16 (regax);
  flag_sub16 (oper1, oper2);
  if (df) {
    putreg16 (regdi, getreg16 (regdi) - 2);
  } else {
    putreg16 (regdi, getreg16 (regdi) + 2);
  }
  if (reptype) {
    putreg16 (regcx, getreg16 (regcx) - 1);
  }
  if ( (reptype == 1) && !zf) {
    return;
  } else if ( (reptype == 2) & (zf == 1) ) {
    return;
  }
  totalexec++;
  loopcount++;
  if (!reptype) {
    return;
  }
  ip = firstip;
}

static inline void opcodeB0 (void) {
  // B0 MOV CPU_AL Ib
  CPU_AL = getmem8 (segregs[regcs], ip);
  StepIP (1);
}

static inline void opcodeB1 (void) {
  // B1 MOV CPU_CL Ib
  CPU_CL = getmem8 (segregs[regcs], ip);
  StepIP (1);
}

static inline void opcodeB2 (void) {
  // B2 MOV CPU_DL Ib
  CPU_DL = getmem8 (segregs[regcs], ip);
  StepIP (1);
}

static inline void opcodeB3 (void) {
  // B3 MOV CPU_BL Ib
  CPU_BL = getmem8 (segregs[regcs], ip);
  StepIP (1);
}

static inline void opcodeB4 (void) {
  // B4 MOV CPU_AH Ib
  CPU_AH = getmem8 (segregs[regcs], ip);
  StepIP (1);
}

static inline void opcodeB5 (void) {
  // B5 MOV CPU_CH Ib
  CPU_CH = getmem8 (segregs[regcs], ip);
  StepIP (1);
}

static inline void opcodeB6 (void) {
  // B6 MOV CPU_DH Ib
  CPU_DH = getmem8 (segregs[regcs], ip);
  StepIP (1);
}

static inline void opcodeB7 (void) {
  // B7 MOV CPU_BH Ib
  CPU_BH = getmem8 (segregs[regcs], ip);
  StepIP (1);
}

static inline void opcodeB8 (void) {
  // B8 MOV eAX Iv
  oper1 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  putreg16 (regax, oper1);
}

static inline void opcodeB9 (void) {
  // B9 MOV eCX Iv
  oper1 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  putreg16 (regcx, oper1);
}

static inline void opcodeBA (void) {
  // BA MOV eDX Iv
  oper1 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  putreg16 (regdx, oper1);
}

static inline void opcodeBB (void) {
  // BB MOV eBX Iv
  oper1 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  putreg16 (regbx, oper1);
}

static inline void opcodeBC (void) {
  // BC MOV eSP Iv
  putreg16 (regsp, getmem16 (segregs[regcs], ip) );
  StepIP (2);
}

static inline void opcodeBD (void) {
  // BD MOV eBP Iv
  putreg16 (regbp, getmem16 (segregs[regcs], ip) );
  StepIP (2);
}

static inline void opcodeBE (void) {
  // BE MOV eSI Iv
  putreg16 (regsi, getmem16 (segregs[regcs], ip) );
  StepIP (2);
}

static inline void opcodeBF (void) {
  // BF MOV eDI Iv
  putreg16 (regdi, getmem16 (segregs[regcs], ip) );
  StepIP (2);
}

static inline void opcodeC0 (void) {
  // C0 GRP2 byte imm8 (80186+)
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  writerm8 (rm, op_grp2_8 (oper2b) );
}

static inline void opcodeC1 (void) {
  // C1 GRP2 word imm8 (80186+)
  modregrm();
  oper1 = readrm16 (rm);
  oper2 = getmem8 (segregs[regcs], ip);
  StepIP (1);
  writerm16 (rm, op_grp2_16 ( (uint8_t) oper2) );
}

static inline void opcodeC2 (void) {
  // C2 RET Iw
  oper1 = getmem16 (segregs[regcs], ip);
  ip = pop();
  putreg16 (regsp, getreg16 (regsp) + oper1);
}

static inline void opcodeC3 (void) {
  // C3 RET
  ip = pop();
}

static inline void opcodeC4 (void) {
  // C4 LES Gv Mp
  modregrm();
  getea (rm);
  putreg16 (reg, read86 (ea) + read86 (ea + 1) * 256);
  segregs[reges] = read86 (ea + 2) + read86 (ea + 3) * 256;
}

static inline void opcodeC5 (void) {
  // C5 LDS Gv Mp
  modregrm();
  getea (rm);
  putreg16 (reg, read86 (ea) + read86 (ea + 1) * 256);
  segregs[regds] = read86 (ea + 2) + read86 (ea + 3) * 256;
}

static inline void opcodeC6 (void) {
  // C6 MOV Eb Ib
  modregrm();
  writerm8 (rm, getmem8 (segregs[regcs], ip) );
  StepIP (1);
}

static inline void opcodeC7 (void) {
  // C7 MOV Ev Iv
  modregrm();
  writerm16 (rm, getmem16 (segregs[regcs], ip) );
  StepIP (2);
}

static inline void opcodeC8 (void) {
  // C8 ENTER (80186+)
  stacksize = getmem16 (segregs[regcs], ip);
  StepIP (2);
  nestlev = getmem8 (segregs[regcs], ip);
  StepIP (1);
  push (getreg16 (regbp) );
  frametemp = getreg16 (regsp);
  if (nestlev) {
    for (temp16 = 1; temp16 < nestlev; temp16++) {
      putreg16 (regbp, getreg16 (regbp) - 2);
      push (getreg16 (regbp) );
    }
    push (getreg16 (regsp) );
  }
  putreg16 (regbp, frametemp);
  putreg16 (regsp, getreg16 (regbp) - stacksize);
}

static inline void opcodeC9 (void) {
  // C9 LEAVE (80186+)
  putreg16 (regsp, getreg16 (regbp) );
  putreg16 (regbp, pop() );
}

static inline void opcodeCA (void) {
  // CA RETF Iw
  oper1 = getmem16 (segregs[regcs], ip);
  ip = pop();
  segregs[regcs] = pop();
  putreg16 (regsp, getreg16 (regsp) + oper1);
}

static inline void opcodeCB (void) {
  // CB RETF
  ip = pop();;
  segregs[regcs] = pop();
}

static inline void opcodeCC (void) {
  // CC INT 3
  intcall86 (3);
}

static inline void opcodeCD (void) {
  // CD INT Ib
  oper1b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  intcall86 (oper1b);
}

static inline void opcodeCE (void) {
  // CE INTO
  if (of) {
    intcall86 (4);
  }
}

static inline void opcodeCF (void) {
  // CF IRET
  ip = pop();
  segregs[regcs] = pop();
  decodeflagsword (pop() );
   // if (net.enabled) net.canrecv = 1;

}

static inline void opcodeD0 (void) {
  // D0 GRP2 Eb 1
  modregrm();
  oper1b = readrm8 (rm);
  writerm8 (rm, op_grp2_8 (1) );
}

static inline void opcodeD1 (void) {
  // D1 GRP2 Ev 1
  modregrm();
  oper1 = readrm16 (rm);
  writerm16 (rm, op_grp2_16 (1) );
}

static inline void opcodeD2 (void) {
  // D2 GRP2 Eb CPU_CL
  modregrm();
  oper1b = readrm8 (rm);
  writerm8 (rm, op_grp2_8 (CPU_CL) );
}

static inline void opcodeD3 (void) {
  // D3 GRP2 Ev CPU_CL
  modregrm();
  oper1 = readrm16 (rm);
  writerm16 (rm, op_grp2_16 (CPU_CL) );
}

static inline void opcodeD4 (void) {
  // D4 AAM I0
  oper1 = getmem8 (segregs[regcs], ip);
  StepIP (1);
  if (!oper1) {
    intcall86 (0);
      return;
  } // division by zero

  CPU_AH = (CPU_AL / oper1) & 255;
  CPU_AL = (CPU_AL % oper1) & 255;
  flag_szp16 (getreg16 (regax) );
}

static inline void opcodeD5 (void) {
  // D5 AAD I0
  oper1 = getmem8 (segregs[regcs], ip);
  StepIP (1);
  CPU_AL = (CPU_AH * oper1 + CPU_AL) & 255;
  CPU_AH = 0;
  flag_szp16 (CPU_AH * oper1 + CPU_AL);
  sf = 0;
}

static inline void opcodeD6 (void) {
  // D6 XLAT on V20/V30, SALC on 8086/8088
#ifndef CPU_V20
  CPU_AL = cf ? 0xFF : 0x00;
#endif
}

static inline void opcodeD7 (void) {
  // D7 XLAT
  CPU_AL = read86(useseg * 16 + (CPU_BX) + CPU_AL);
}

static inline void opcodeD8 (void) {
  // escape to x87 FPU (unsupported)
  modregrm();
}
static inline void opcodeD9 (void) {
  // escape to x87 FPU (unsupported)
  modregrm();
}
static inline void opcodeDA (void) {
  // escape to x87 FPU (unsupported)
  modregrm();
}
static inline void opcodeDB (void) {
  // escape to x87 FPU (unsupported)
  modregrm();
}
static inline void opcodeDC (void) {
  // escape to x87 FPU (unsupported)
  modregrm();
}
static inline void opcodeDD (void) {
  // escape to x87 FPU (unsupported)
  modregrm();
}
static inline void opcodeDE (void) {
  // escape to x87 FPU (unsupported)
  modregrm();
}
static inline void opcodeDF (void) {
  // escape to x87 FPU (unsupported)
  modregrm();
}

static inline void opcodeE0 (void) {
  // E0 LOOPNZ Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  putreg16 (regcx, getreg16 (regcx) - 1);
  if ( (getreg16 (regcx) ) && !zf) {
    ip = ip + temp16;
  }
}

static inline void opcodeE1 (void) {
  // E1 LOOPZ Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  putreg16 (regcx, (getreg16 (regcx) ) - 1);
  if ( (getreg16 (regcx) ) && (zf == 1) ) {
    ip = ip + temp16;
  }
}

static inline void opcodeE2 (void) {
  // E2 LOOP Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  putreg16 (regcx, (getreg16 (regcx) ) - 1);
  if (getreg16 (regcx) ) {
    ip = ip + temp16;
  }
}

static inline void opcodeE3 (void) {
  // E3 JCXZ Jb
  temp16 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  if (! (getreg16 (regcx) ) ) {
    ip = ip + temp16;
  }
}

static inline void opcodeE4 (void) {
  // E4 IN CPU_AL Ib
  oper1b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  CPU_AL = (uint8_t) portin (oper1b);
}

static inline void opcodeE5 (void) {
  // E5 IN eAX Ib
  oper1b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  putreg16 (regax, portin16 (oper1b) );
}

static inline void opcodeE6 (void) {
  // E6 OUT Ib CPU_AL
  oper1b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  portout (oper1b, CPU_AL);
}

static inline void opcodeE7 (void) {
  // E7 OUT Ib eAX
  oper1b = getmem8 (segregs[regcs], ip);
  StepIP (1);
  portout16 (oper1b, (getreg16 (regax) ) );
}

static inline void opcodeE8 (void) {
  // E8 CALL Jv
  oper1 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  push (ip);
  ip = ip + oper1;
}

static inline void opcodeE9 (void) {
  // E9 JMP Jv
  oper1 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  ip = ip + oper1;
}

static inline void opcodeEA (void) {
  // EA JMP Ap
  oper1 = getmem16 (segregs[regcs], ip);
  StepIP (2);
  oper2 = getmem16 (segregs[regcs], ip);
  ip = oper1;
  segregs[regcs] = oper2;
}

static inline void opcodeEB (void) {
  // EB JMP Jb
  oper1 = signext (getmem8 (segregs[regcs], ip) );
  StepIP (1);
  ip = ip + oper1;
}

static inline void opcodeEC (void) {
  // EC IN CPU_AL regdx
  oper1 = (getreg16 (regdx) );
  CPU_AL = (uint8_t) portin (oper1);
}

static inline void opcodeED (void) {
  // ED IN eAX regdx
  oper1 = (getreg16 (regdx) );
  putreg16 (regax, portin16 (oper1) );
}

static inline void opcodeEE (void) {
  // EE OUT regdx CPU_AL
  oper1 = (getreg16 (regdx) );
  portout (oper1, CPU_AL);
}

static inline void opcodeEF (void) {
  // EF OUT regdx eAX
  oper1 = (getreg16 (regdx) );
  portout16 (oper1, (getreg16 (regax) ) );
}

static inline void opcodeF0 (void) {
  // F0 LOCK
}

static inline void opcodeF4 (void) {
  // F4 HLT
  ip--;
}

static inline void opcodeF5 (void) {
  // F5 CMC
  if (!cf) {
      cf = 1;
  } else {
    cf = 0;
  }
}

static inline void opcodeF6 (void) {
  // F6 GRP3a Eb
  modregrm();
  oper1b = readrm8 (rm);
  op_grp3_8();
  if ( (reg > 1) && (reg < 4) ) {
    writerm8 (rm, res8);
  }
}

static inline void opcodeF7 (void) {
  // F7 GRP3b Ev
  modregrm();
  oper1 = readrm16 (rm);
  op_grp3_16();
  if ( (reg > 1) && (reg < 4) ) {
    writerm16 (rm, res16);
  }
}

static inline void opcodeF8 (void) {
  // F8 CLC
  cf = 0;
}

static inline void opcodeF9 (void) {
  // F9 STC
  cf = 1;
}

static inline void opcodeFA (void) {
  // FA CLI
  ifl = 0;
}

static inline void opcodeFB (void) {
  // FB STI
  ifl = 1;
}

static inline void opcodeFC (void) {
  // FC CLD
  df = 0;
}

static inline void opcodeFD (void) {
  // FD STD
  df = 1;
}

static inline void opcodeFE (void) {
  // FE GRP4 Eb
  modregrm();
  oper1b = readrm8 (rm);
  oper2b = 1;
  if (!reg) {
    tempcf = cf;
    res8 = oper1b + oper2b;
    flag_add8 (oper1b, oper2b);
    cf = tempcf;
    writerm8 (rm, res8);
  } else {
    tempcf = cf;
    res8 = oper1b - oper2b;
    flag_sub8 (oper1b, oper2b);
    cf = tempcf;
    writerm8 (rm, res8);
  }
}

static inline void opcodeFF (void) {
  // FF GRP5 Ev
  modregrm();
  oper1 = readrm16 (rm);
  op_grp5();
}

#ifndef CPU_V20
static inline void opcode60 (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode61 (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode62 (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode68 (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode69 (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode6A (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode6B (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode6C (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode6D (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode6E (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode6F (void) {  //unknown
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
#endif

static inline void opcode26 (void) {  //unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode2E (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode36 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode3E (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode63 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode64 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode65 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode66 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcode67 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcodeF1 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcodeF2 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}
static inline void opcodeF3 (void) {  ///unknown
#ifdef CPU_V20
  intcall86 (6);  // trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs.
                  // technically they aren't exactly like NOPs in most cases, but for our pursoses, that's accurate enough.
#endif
#if DEBUG > 0
    printf ("Illegal opcode: %02X @ %04X:%04X\n", opcode, savecs, saveip);
#endif
}

typedef void (*Handler)(void);    // A pointer to a handler function
static Handler jump_table[256] = {
  opcode00,opcode01,opcode02,opcode03,opcode04,opcode05,opcode06,opcode07,opcode08,opcode09,opcode0A,opcode0B,opcode0C,opcode0D,opcode0E,opcode0F,
  opcode10,opcode11,opcode12,opcode13,opcode14,opcode15,opcode16,opcode17,opcode18,opcode19,opcode1A,opcode1B,opcode1C,opcode1D,opcode1E,opcode1F,
  opcode20,opcode21,opcode22,opcode23,opcode24,opcode25,opcode26,opcode27,opcode28,opcode29,opcode2A,opcode2B,opcode2C,opcode2D,opcode2E,opcode2F,
  opcode30,opcode31,opcode32,opcode33,opcode34,opcode35,opcode36,opcode37,opcode38,opcode39,opcode3A,opcode3B,opcode3C,opcode3D,opcode3E,opcode3F,
  opcode40,opcode41,opcode42,opcode43,opcode44,opcode45,opcode46,opcode47,opcode48,opcode49,opcode4A,opcode4B,opcode4C,opcode4D,opcode4E,opcode4F,
  opcode50,opcode51,opcode52,opcode53,opcode54,opcode55,opcode56,opcode57,opcode58,opcode59,opcode5A,opcode5B,opcode5C,opcode5D,opcode5E,opcode5F,
  opcode60,opcode61,opcode62,opcode63,opcode64,opcode65,opcode66,opcode67,opcode68,opcode69,opcode6A,opcode6B,opcode6C,opcode6D,opcode6E,opcode6F,
  opcode70,opcode71,opcode72,opcode73,opcode74,opcode75,opcode76,opcode77,opcode78,opcode79,opcode7A,opcode7B,opcode7C,opcode7D,opcode7E,opcode7F,
  opcode80,opcode81,opcode82,opcode83,opcode84,opcode85,opcode86,opcode87,opcode88,opcode89,opcode8A,opcode8B,opcode8C,opcode8D,opcode8E,opcode8F,
  opcode90,opcode91,opcode92,opcode93,opcode94,opcode95,opcode96,opcode97,opcode98,opcode99,opcode9A,opcode9B,opcode9C,opcode9D,opcode9E,opcode9F,
  opcodeA0,opcodeA1,opcodeA2,opcodeA3,opcodeA4,opcodeA5,opcodeA6,opcodeA7,opcodeA8,opcodeA9,opcodeAA,opcodeAB,opcodeAC,opcodeAD,opcodeAE,opcodeAF,
  opcodeB0,opcodeB1,opcodeB2,opcodeB3,opcodeB4,opcodeB5,opcodeB6,opcodeB7,opcodeB8,opcodeB9,opcodeBA,opcodeBB,opcodeBC,opcodeBD,opcodeBE,opcodeBF,
  opcodeC0,opcodeC1,opcodeC2,opcodeC3,opcodeC4,opcodeC5,opcodeC6,opcodeC7,opcodeC8,opcodeC9,opcodeCA,opcodeCB,opcodeCC,opcodeCD,opcodeCE,opcodeCF,
  opcodeD0,opcodeD1,opcodeD2,opcodeD3,opcodeD4,opcodeD5,opcodeD6,opcodeD7,opcodeD8,opcodeD9,opcodeDA,opcodeDB,opcodeDC,opcodeDD,opcodeDE,opcodeDF,
  opcodeE0,opcodeE1,opcodeE2,opcodeE3,opcodeE4,opcodeE5,opcodeE6,opcodeE7,opcodeE8,opcodeE9,opcodeEA,opcodeEB,opcodeEC,opcodeED,opcodeEE,opcodeEF,
  opcodeF0,opcodeF1,opcodeF2,opcodeF3,opcodeF4,opcodeF5,opcodeF6,opcodeF7,opcodeF8,opcodeF9,opcodeFA,opcodeFB,opcodeFC,opcodeFD,opcodeFE,opcodeFF
};

void DOOPCODE(uint8_t opcode) {
  jump_table[opcode]();
}
void exec86(uint32_t execloops) {
    uint8_t docontinue;
    static uint16_t trap_toggle = 0;

    //counterticks = (uint64_t) ( (double) timerfreq / (double) 65536.0);
    //tickssource();
    for (loopcount = 0; loopcount < execloops; loopcount++) {
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
#ifdef XMS_DRIVER
            // W/A-hack: last byte of interrupts table (actually should not be ever used as CS:IP)
            if (CPU_CS == XMS_FN_CS && ip == XMS_FN_IP) {
                // hook for XMS
                opcode = xms_fn(); // always returns RET TODO: far/short ret?
            }
            else {
                opcode = getmem8(CPU_CS, ip);
            }
#else
            opcode = getmem8(CPU_CS, ip);
#endif
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

        jump_table[opcode]();
#if 0
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
#endif
    }
}

// type of 8-bit write function pointer
typedef void (*write_fn_ptr)(uint32_t, uint8_t);
// array of function pointers separated by 800h (32K) pages (less gradation to be implemented by "if" conditions)
static write_fn_ptr write_funtions[256] = { 0 };

static inline void write8video(uint32_t addr32, uint8_t v) {
    VIDEORAM[(ega_plane_offset + addr32 - VIDEORAM_START32) % VIDEORAM_SIZE] = v;
}
#ifdef XMS_UMB
static void write8umb_psram(uint32_t addr32, uint8_t v) {
    if (umb_in_use(addr32)) {
        //char tmp[80]; sprintf(tmp, "UMB W8 %08X <- %02X", addr32, v); logMsg(tmp);
        write8psram(addr32, v);
    }
}

static void write8umb_swap(uint32_t addr32, uint8_t v) {
    if (umb_in_use(addr32)) {
        ram_page_write(addr32, v);
    }
}
#endif
static void write8hma_psram(uint32_t addr32, uint8_t v) {
    if (a20_line_open) {
        // A20 line is ON
        write8psram(addr32, v);
        return;
    }
    write86(addr32 - HMA_START_ADDRESS, v); // Rool back to low addressed
}

static void write8nohma(uint32_t addr32, uint8_t v) {
    write86(addr32 - HMA_START_ADDRESS, v); // Rool back to low addressed
}

static void write8hma_swap(uint32_t addr32, uint8_t v) {
    if (a20_line_open) {
        // A20 line is ON
        ram_page_write(addr32, v);
        return;
    }
    write86(addr32 - HMA_START_ADDRESS, v); // Rool back to low addressed
}
#ifdef EMS_DRIVER
static void write8emm_psram(uint32_t addr32, uint8_t v) {
    uint32_t lba = get_logical_lba_for_physical_lba(addr32);
    if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
        write8psram(lba, v);
    }
}

static void write8emm_swap(uint32_t addr32, uint8_t v) {
    uint32_t lba = get_logical_lba_for_physical_lba(addr32);
    if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
        ram_page_write(lba, v);
    }
}
#endif
// This function is required to place into "empty" clots, to avoid null ptr functions
static void write8nothing(uint32_t addr32, uint8_t v) {
    // do nothing
}

static void write8low(uint32_t addr32, uint8_t v) {
    if (addr32 < RAM_SIZE) {
        RAM[addr32] = v;
    }
}

static void write8low_psram(uint32_t addr32, uint8_t v) {
    if (addr32 < RAM_SIZE) {
        RAM[addr32] = v;
    } else {
        write8psram(addr32, v);
    }
}

static void write8low_swap(uint32_t addr32, uint8_t v) {
    if (addr32 < RAM_PAGE_SIZE) {
        RAM[addr32] = v;
    } else {
        ram_page_write(addr32, v);
    }
}


// type of 16-bit write function pointer
typedef void (*write16_fn_ptr)(uint32_t, uint16_t);
// array of function pointers separated by 800h (32K) pages (less gradation to be implemented by "if" conditions)
static write16_fn_ptr write16_funtions[256] = { 0 };

static inline void write16arr(uint8_t* arr, uint32_t base_addr, uint32_t addr32, uint16_t value) {
    register uint8_t* ptr = arr - base_addr + addr32;
    *ptr++ = (uint8_t)value;
    *ptr = (uint8_t)(value >> 8);
}

static inline void write16video(uint32_t addr32, uint16_t v) {
    write16arr(VIDEORAM, 0, (ega_plane_offset + addr32 - VIDEORAM_START32) % VIDEORAM_SIZE, v);
}
#ifdef XMS_DRIVER
#ifdef XMS_UMB
static void write16umb_psram(uint32_t addr32, uint16_t v) {
    if (umb_in_use(addr32)) {
        //char tmp[80]; sprintf(tmp, "UMB W16 %08X <- %04X", addr32, v); logMsg(tmp);
        write16psram(addr32, v);
    }
}

static void write16umb_swap(uint32_t addr32, uint16_t v) {
    if (umb_in_use(addr32)) {
        ram_page_write16(addr32, v);
    }
}
#endif
#endif
static void write16hma_psram(uint32_t addr32, uint16_t v) {
    if (a20_line_open) {
        // A20 line is ON
        write16psram(addr32, v);
        return;
    }
    writew86(addr32 - HMA_START_ADDRESS, v); // Rool back to low addressed
}

static void write16nohma(uint32_t addr32, uint16_t v) {
    writew86(addr32 - HMA_START_ADDRESS, v); // Rool back to low addressed
}

static void write16hma_swap(uint32_t addr32, uint16_t v) {
    if (a20_line_open) {
        // A20 line is ON
        ram_page_write16(addr32, v);
        return;
    }
    writew86(addr32 - HMA_START_ADDRESS, v); // Rool back to low addressed
}
#ifdef EMS_DRIVER
static void write16emm_psram(uint32_t addr32, uint16_t v) {
    uint32_t lba = get_logical_lba_for_physical_lba(addr32);
    if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
        write16psram(lba, v);
    }
}

static void write16emm_swap(uint32_t addr32, uint16_t v) {
    uint32_t lba = get_logical_lba_for_physical_lba(addr32);
    if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
        ram_page_write16(lba, v);
    }
}
#endif
// This function is required to place into "empty" clots, to avoid null ptr functions
static void write16nothing(uint32_t addr32, uint16_t v) {
    // do nothing
}

static void write16low(uint32_t addr32, uint16_t v) {
    if (addr32 < RAM_SIZE) {
        write16arr(RAM, 0, addr32, v);
    }
}

static void write16low_psram(uint32_t addr32, uint16_t v) {
    if (addr32 < RAM_SIZE) {
        write16arr(RAM, 0, addr32, v);
    } else {
        write16psram(addr32, v);
    }
}

static void write16low_swap(uint32_t addr32, uint16_t v) {
    if (addr32 < RAM_PAGE_SIZE) {
        write16arr(RAM, 0, addr32, v);
    } else {
        ram_page_write16(addr32, v);
    }
}

// type of 8-bit read function pointer
typedef uint8_t (*read_fn_ptr)(uint32_t);
// array of function pointers separated by 800h (32K) pages (less gradation to be implemented by "if" conditions)
static read_fn_ptr read_funtions[256] = { 0 };

uint8_t read8nothng(uint32_t addr32) {
    return 0;
}

static inline uint8_t read8video(uint32_t addr32) {
    return VIDEORAM[(ega_plane_offset + addr32 - VIDEORAM_START32) % VIDEORAM_SIZE];
}

static inline uint8_t read86rom(uint32_t addr32) {
    if ((addr32 >= 0xFE000UL) && (addr32 <= 0xFFFFFUL)) {
        // BIOS ROM range
        return BIOS[addr32 - 0xFE000UL];
    }
    if ((addr32 >= 0xF6000UL) && (addr32 < 0xFA000UL)) {
        // IBM BASIC ROM LOW
        return BASICL[addr32 - 0xF6000UL];
    }
    if ((addr32 >= 0xFA000UL) && (addr32 < 0xFE000UL)) {
        // IBM BASIC ROM HIGH
        return BASICH[addr32 - 0xFA000UL];
    }
    return 0;
}

#ifdef XMS_DRIVER
#ifdef XMS_UMB
uint8_t read8umb_psram(uint32_t addr32) {
    if (umb_in_use(addr32)) {
        uint8_t v = read8psram(addr32);
        //char tmp[80]; sprintf(tmp, "UMB R8 %08X -> %02X", addr32, v); logMsg(tmp);
        return v;
    }
    return read86rom(addr32);
}

uint8_t read8umb_swap(uint32_t addr32) {
    if (umb_in_use(addr32)) {
        return ram_page_read(addr32);
    }
    return read86rom(addr32);
}
#endif
#endif
uint8_t read8hma_psram(uint32_t addr32) {
    if (a20_line_open) {
        return read8psram(addr32);
    }
    return read86(addr32 - HMA_START_ADDRESS); // FFFF:0010 -> 0000:0000 rolling address space for case A20 is turned off
}

uint8_t read8nohma(uint32_t addr32) {
    return read86(addr32 - HMA_START_ADDRESS); // FFFF:0010 -> 0000:0000 rolling address space for case A20 is turned off
}

uint8_t read8hma_swap(uint32_t addr32) {
    if (a20_line_open) {
        return ram_page_read(addr32);
    }
    return read86(addr32 - HMA_START_ADDRESS); // FFFF:0010 -> 0000:0000 rolling address space for case A20 is turned off
}

#ifdef EMS_DRIVER
uint8_t read8emm_psram(uint32_t addr32) {
    uint32_t lba = get_logical_lba_for_physical_lba(addr32);
    if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
        return read8psram(lba);
    }
    return read86rom(addr32);
}

uint8_t read8emm_swap(uint32_t addr32) {
    uint32_t lba = get_logical_lba_for_physical_lba(addr32);
    if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
        return ram_page_read(lba);
    }
    return read86rom(addr32);
}
#endif
// type of 16-bit read function pointer
typedef uint16_t (*read16_fn_ptr)(uint32_t);
// array of function pointers separated by 800h (32K) pages (less gradation to be implemented by "if" conditions)
static read16_fn_ptr read16_funtions[256] = { 0 };

static uint16_t read16nothng(uint32_t addr32) {
    return 0;
}

static __inline uint16_t read16arr(uint8_t* arr, uint32_t base_addr, uint32_t addr32) {
    arr += addr32 - base_addr;
    register uint16_t b1 = *arr++;
    register uint16_t b0 = *arr;
    return b1 | (b0 << 8);
}

static __inline uint16_t read16arr0(uint8_t* arr, uint32_t addr32) {
    arr += addr32;
    register uint16_t b1 = *arr++;
    register uint16_t b0 = *arr;
    return b1 | (b0 << 8);
}

static inline uint16_t read16video(uint32_t addr32) {
    return read16arr0(VIDEORAM, (ega_plane_offset + addr32 - VIDEORAM_START32) % VIDEORAM_SIZE);
}

static inline uint16_t read86rom16(uint32_t addr32) {
    if (addr32 >= 0xFE000UL && addr32 <= 0xFFFFFUL) {
        // BIOS ROM range
        return read16arr(BIOS, 0xFE000UL, addr32);
    }
    if (addr32 >= 0xF6000UL && addr32 < 0xFA000UL) {
        // IBM BASIC ROM LOW
        return read16arr(BASICL, 0xF6000U, addr32);
    }
    if ((addr32 >= 0xFA000UL) && (addr32 < 0xFE000UL)) {
        // IBM BASIC ROM HIGH
        return read16arr(BASICH, 0xFA000UL, addr32);
    }
    return 0;
}
#ifdef XMS_DRIVER
#ifdef XMS_UMB
uint16_t read16umb_psram(uint32_t addr32) {
    if (umb_in_use(addr32)) {
        return read16psram(addr32);
    }
    return read86rom16(addr32);
}

uint16_t read16umb_swap(uint32_t addr32) {
    if (umb_in_use(addr32)) {
        uint16_t v = ram_page_read16(addr32);
        //char tmp[80]; sprintf(tmp, "UMB R16 %08X -> %04X", addr32, v); logMsg(tmp);
        return v;
    }
    return read86rom16(addr32);
}
#endif
#endif
uint16_t read16hma_psram(uint32_t addr32) {
    if (a20_line_open) {
        return read16psram(addr32);
    }
    return readw86(addr32 - HMA_START_ADDRESS); // FFFF:0010 -> 0000:0000 rolling address space for case A20 is turned off
}

uint16_t read16nohma(uint32_t addr32) {
    return readw86(addr32 - HMA_START_ADDRESS); // FFFF:0010 -> 0000:0000 rolling address space for case A20 is turned off
}

uint16_t read16hma_swap(uint32_t addr32) {
    if (a20_line_open) {
        return ram_page_read16(addr32);
    }
    return readw86(addr32 - HMA_START_ADDRESS); // FFFF:0010 -> 0000:0000 rolling address space for case A20 is turned off
}

#ifdef EMS_DRIVER
uint16_t read16emm_psram(uint32_t addr32) {
    uint32_t lba = get_logical_lba_for_physical_lba(addr32);
    if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
        return read16psram(lba);
    }
    return read86rom16(addr32);
}

uint16_t read16emm_swap(uint32_t addr32) {
    uint32_t lba = get_logical_lba_for_physical_lba(addr32);
    if (lba >= (EMM_LBA_SHIFT_KB << 10)) {
        return ram_page_read16(lba);
    }
    return read86rom16(addr32);
}
#endif

void init_cpu_addresses_map() {
    // just init all array positions to avoid hangs in gaps
    uint8_t ba = 0; do {
        write_funtions  [ba] = write8nothing ;
        write16_funtions[ba] = write16nothing;
        read_funtions   [ba] = read8nothng   ;
        read16_funtions [ba] = read16nothng  ;
    } while (++ba);
#if  1 //PICO_ON_DEVICE
    // override for known pages
    for (uint8_t ba = 0; ba <= (CONVENTIONAL_END >> 15); ++ba) {
        write_funtions  [ba] = PSRAM_AVAILABLE ? write8low_psram  : (SD_CARD_AVAILABLE ? write8low_swap  : write8low   );
        write16_funtions[ba] = PSRAM_AVAILABLE ? write16low_psram : (SD_CARD_AVAILABLE ? write16low_swap : write16low  );
        read_funtions   [ba] = PSRAM_AVAILABLE ? read8psram       : (SD_CARD_AVAILABLE ? ram_page_read   : read8nothng );
        read16_funtions [ba] = PSRAM_AVAILABLE ? read16psram      : (SD_CARD_AVAILABLE ? ram_page_read16 : read16nothng);
    }
    // CONVENTIONAL_END == VIDEORAM_START32
    for (uint8_t ba = (VIDEORAM_START32 >> 15); ba <= (VIDEORAM_END32 >> 15); ++ba) {
        write_funtions  [ba] = write8video ;
        write16_funtions[ba] = write16video;
        read_funtions   [ba] = read8video  ;
        read16_funtions [ba] = read16video ;
    }
#ifdef XMS_DRIVER
  #ifdef XMS_UMB
    // UMB_START_ADDRESS == VIDEORAM_END32
    for (uint8_t ba = (VIDEORAM_END32 >> 15); ba <= (HMA_START_ADDRESS >> 15); ++ba) {
        write_funtions  [ba] = PSRAM_AVAILABLE ? write8umb_psram  : write8umb_swap ;
        write16_funtions[ba] = PSRAM_AVAILABLE ? write16umb_psram : write16umb_swap;
        read_funtions   [ba] = PSRAM_AVAILABLE ? read8umb_psram   : read8umb_swap  ;
        read16_funtions [ba] = PSRAM_AVAILABLE ? read16umb_psram  : read16umb_swap  ;
    }
  #else
    for (uint8_t ba = (VIDEORAM_END32 >> 15); ba <= (HMA_START_ADDRESS >> 15); ++ba) {
        read_funtions   [ba] = read86rom;
        read16_funtions [ba] = read86rom16;
    }
  #endif
  #ifdef XMS_HMA
    for (uint8_t ba = (HMA_START_ADDRESS >> 15); ba <= (BASE_XMS_ADDR >> 15); ++ba) {
        write_funtions  [ba] = PSRAM_AVAILABLE ? write8hma_psram  : write8hma_swap ;
        write16_funtions[ba] = PSRAM_AVAILABLE ? write16hma_psram : write16hma_swap;
        read_funtions   [ba] = PSRAM_AVAILABLE ? read8hma_psram   : read8hma_swap  ;
        read16_funtions [ba] = PSRAM_AVAILABLE ? read16hma_psram  : read16hma_swap ;
    }
  #else
    for (uint8_t ba = (HMA_START_ADDRESS >> 15); ba <= (BASE_XMS_ADDR >> 15); ++ba) {
        write_funtions  [ba] = write8nohma ;
        write16_funtions[ba] = write16nohma;
        read_funtions   [ba] = read8nohma  ;
        read16_funtions [ba] = read16nohma ;
    }
  #endif
  char tmp[80];
    for (uint8_t ba = (BASE_XMS_ADDR >> 15); ba <= (ON_BOARD_RAM_KB >> 5); ++ba) {
        write_funtions  [ba] = PSRAM_AVAILABLE ? write8psram  : ram_page_write  ;
        write16_funtions[ba] = PSRAM_AVAILABLE ? write16psram : ram_page_write16;
        read_funtions   [ba] = PSRAM_AVAILABLE ? read8psram   : ram_page_read   ;
        read16_funtions [ba] = PSRAM_AVAILABLE ? read16psram  : ram_page_read16 ;
    }
#else
    for (uint8_t ba = (VIDEORAM_END32 >> 15); ba <= (HMA_START_ADDRESS >> 15); ++ba) {
        read_funtions   [ba] = read86rom;
        read16_funtions [ba] = read86rom16;
    }
    for (uint8_t ba = (VIDEORAM_END32 >> 15); ba <= (HMA_START_ADDRESS >> 15); ++ba) {
        read_funtions   [ba] = read86rom;
        read16_funtions [ba] = read86rom16;
    }
    for (uint8_t ba = (HMA_START_ADDRESS >> 15); ba <= (BASE_XMS_ADDR >> 15); ++ba) {
        write_funtions  [ba] = write8nohma ;
        write16_funtions[ba] = write16nohma;
        read_funtions   [ba] = read8nohma  ;
        read16_funtions [ba] = read16nohma ;
    }
#endif
#ifdef EMS_DRIVER
    // override EMM window
    for (uint8_t ba = (PHYSICAL_EMM_SEGMENT >> 11); ba <= (PHYSICAL_EMM_SEGMENT_END >> 11); ++ba) {
        write_funtions  [ba] = PSRAM_AVAILABLE ? write8emm_psram  : write8emm_swap ;
        write16_funtions[ba] = PSRAM_AVAILABLE ? write16emm_psram : write16emm_swap;
        read_funtions   [ba] = PSRAM_AVAILABLE ? read8emm_psram   : read8emm_swap  ;
        read16_funtions [ba] = PSRAM_AVAILABLE ? read16emm_psram  : read16emm_swap ;
    }
#endif
#else
    for (uint8_t ba = 0; ba <= (CONVENTIONAL_END >> 15); ++ba) {
        write_funtions  [ba] = write8low ;
        write16_funtions[ba] = write16low;
    }
#endif
}

void write86(uint32_t addr32, uint8_t v) {
    write_funtions[addr32 >> 15](addr32, v);
}

void writew86(uint32_t addr32, uint16_t v) {
#ifdef HANDLE_REBOOT
    if (addr32 == 0 && v == 0) {
        // reboot hook
        reboot_detected();
    }
#endif
    if (addr32 & 0x00000001) {
        // not 16-bit aligned - spit to 8-bit ops to avoid page boarders intersection
        write86(addr32    , (uint8_t) v);
        write86(addr32 + 1, (uint8_t)(v >> 8));
    } else {
        write16_funtions[addr32 >> 15](addr32, v);
    }
}

// https://docs.huihoo.com/gnu_linux/own_os/appendix-bios_memory_2.htm
uint8_t read86(uint32_t addr32) {
    // Не удаляй плиз коммент
    //if (addr32 == 0xFC000) { return 0x21; };
    if (addr32 < DIRECT_RAM_BORDER) { // performance improvement (W/A)
        return RAM[addr32];
    }
    return read_funtions[addr32 >> 15](addr32);
}

uint16_t readw86(uint32_t addr32) {
    if ((addr32 & 0x00000001) != 0) {
        // not 16-bit address
        return ((uint16_t)read86(addr32) | (uint16_t)(read86(addr32 + 1) << 8));
    }
    if (addr32 < DIRECT_RAM_BORDER) { // performance improvement (W/A)
        return read16arr0(RAM, addr32);
    }
    return read16_funtions[addr32 >> 15](addr32);
}
