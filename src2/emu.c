/* Connection between ESP-PSRAM64H and ESP8266.  
        pin1       C/S-->GPIO15
        pin2      MISO-->GPIO12
        pin3       N/C-->Gnd
        pin4       Vss-->Gnd
        pin5      MOSI-->GPIO13
        pin6       SCK-->GPIO14
        pin7      HOLD-->V+
        pin8       Vcc-->V+
    
    Configurations:
    CPU frequency = 160MHz
    Flash Mode = QIO
    Flash Frequency = 80MHz

 */

#define FREQUENCY    160
#define DISK_START_ADDRESS  0x100000
#define SWSERBUFCAP 4096
#define SWSERISRCAP 1024
#define CURSOR_SPEED  200                  // blinking at millisecond interval

#include "SDL2/SDL.h"
#include <stdint.h>
#include <stdio.h>
#include <mem.h>
#include "emu.h"
#include "rom.h"
#include "disk.h"
#include "stdbool.h"

char c;
char previous_c;

int wordcount;

char DSbuff[30];

extern uint8_t bootdrive;

uint8_t sectorbuffer1;
uint8_t sectorbuffer2;
uint8_t sectorbuffer3;
uint8_t sectorbuffer4;

uint8_t RAM[1124 << 10];
uint8_t screenmem[16384];

extern uint8_t videomode = 3;

uint32_t tickNumber;
uint32_t tickRemainder;

bool curset = false;
bool curshow = false;
uint8_t CURSOR_ASCII = 95;         // normal = 95, other = box, (not in used)
uint8_t COM1OUT;
uint8_t COM1IN = 0;

uint32_t tempdummy;
uint32_t sbuff[128];
unsigned long currentMillis;

uint8_t opcode, segoverride, reptype, bootdrive, hdcount = 0, fdcount = 0;
uint16_t savecs, saveip, ip, useseg, oldsp;
uint16_t segregs[4], savecs, saveip, ip, useseg, oldsp;
uint8_t tempcf, oldcf, cf, pf, af, zf, sf, tf, ifl, df, of, nt, iopriv, mode, reg, rm, msw = 0;
uint16_t oper1, oper2, res16, disp16, temp16, dummy, stacksize, frametemp;
uint8_t oper1b, oper2b, res8, disp8, temp8, nestlev, addrbyte;
uint16_t cr0 = 0, cr1 = 0, cr2 = 0, cr3 = 0;
uint32_t ldtr = 0, gdtr = 0, gdtlimit = 0, idtr = 0, idtlimit = 0;
uint32_t temp1, temp2, temp3, temp4, temp5, temp32, tempaddr32, ea;
int32_t result, speed = 0;
uint32_t totalexec;
uint16_t *tempwordptr;
uint16_t portram[256];

uint16_t pit0counter = 65535;
uint32_t speakercountdown = 1320;                                   // random value to start with
uint32_t latch42, pit0latch, pit0command, pit0divisor;

uint8_t crt_controller_idx, crt_controller[256];
uint16_t divisorLatchWord;

uint8_t pr3FArst = 0;
uint16_t pr3F8;
uint16_t pr3F9;
uint16_t pr3FA;
uint16_t pr3FB;
uint16_t pr3FC;
uint16_t pr3FD;
uint16_t pr3FE;
uint16_t pr3FF;
uint8_t pr3D9;

#include <windows.h>
#include <time.h>

int cursor_x = 0;
int cursor_y = 0;
static int color = 7;
static void bios_putchar(const char c) {
    //printf("\033[%im%c", color, c);
    if (c == 0x0D) {
        cursor_x = 0;
        cursor_y++;
    } else if (c == 0x0A) {
        cursor_x = 0;
    } else if (c == 0x08 && cursor_x > 0) {
        cursor_x--;
        screenmem[/*0xB8000 + */(cursor_y * 160) + cursor_x * 2 + 0] = 32;
        screenmem[/*0xB8000 + */(cursor_y * 160) + cursor_x * 2 + 1] = color;
    } else {
        screenmem[/*0xB8000 + */(cursor_y * 160) + cursor_x * 2 + 0] = c & 0xFF;
        screenmem[/*0xB8000 + */(cursor_y * 160) + cursor_x * 2 + 1] = color;
        if (cursor_x == 79) {
            cursor_x = 0;
            cursor_y++;
        } else
            cursor_x++;
    }

    if (cursor_y == 25) {
        cursor_y = 24;

        memmove(screenmem/* + 0xB8000*/, screenmem /*+ 0xB8000*/ + 160, 80 * 25 * 2);
        for (int a = 0; a < 80; a++) {
            screenmem[/*0xB8000 + */24 * 160 + a * 2 + 0] = 32;
            screenmem[/*0xB8000 + */24 * 160 + a * 2 + 1] = color;

        }
    }
}


uint8_t random() {
    // Seed the random number generator with current time
    srand(time(NULL));

    // Generate a random uint8_t value
    uint8_t random_value = rand() % 256;

    return random_value;
}

unsigned long millis() {
    static unsigned long start_time = 0;
    static bool timer_initialized = false;
    SYSTEMTIME current_time;
    FILETIME file_time;
    unsigned long long time_now;

    if (!timer_initialized) {
        GetSystemTime(&current_time);
        SystemTimeToFileTime(&current_time, &file_time);
        start_time =
                (((unsigned long long) file_time.dwHighDateTime) << 32) | (unsigned long long) file_time.dwLowDateTime;
        timer_initialized = true;
    }

    GetSystemTime(&current_time);
    SystemTimeToFileTime(&current_time, &file_time);
    time_now = (((unsigned long long) file_time.dwHighDateTime) << 32) | (unsigned long long) file_time.dwLowDateTime;

    return (unsigned long) ((time_now - start_time)/* / 10000*/);
}


void CopyCharROM() {
    int k;
    for (k = 0; k < 2048; k++) {
        screenmem[14336 + k] = charROM[k];
    }
}

//============================================================================= CPU ==========================================================================================

extern void portout(uint16_t portnum, uint16_t value);

extern uint16_t portin(uint16_t portnum);

extern void doirq(uint8_t irqnum);

extern uint8_t nextintr();

extern struct structpic {
    uint8_t imr; //mask register
    uint8_t irr; //request register
    uint8_t isr; //service register
    uint8_t icwstep; //used during initialization to keep track of which ICW we're at
    uint8_t icw[5];
    uint8_t intoffset; //interrupt vector offset
    uint8_t priority; //which IRQ has highest priority
    uint8_t autoeoi; //automatic EOI mode
    uint8_t readmode; //remember what to return on read register from OCW3
    uint8_t enabled;
} i8259;

extern uint8_t curkey;
uint8_t curkey1;

void intcall86(uint8_t intnum);

uint64_t curtimer, lasttimer, timerfreq;

char *biosfile = NULL;
uint8_t byteregtable[8] = { regal, regcl, regdl, regbl, regah, regch, regdh, regbh };
uint8_t parity[0x100];
uint8_t vidmode, cgabg, blankattr, vidgfxmode, vidcolor;
uint16_t cursx, cursy, cols, rows, vgapage, cursorposition, cursorvisible;
uint8_t updatedscreen, port3da, port6, portout16;
uint32_t videobase, textbase, x, y;
uint8_t debugmode, showcsip, verbose, mouseemu;
union _bytewordregs_ regs;

inline void SRAM_write(uint32_t addr32, uint8_t value) {
    RAM[addr32] = value;
}

void write86(uint32_t addr32, uint8_t value) {
    if ((addr32) < SRAM_SIZE) {
        SRAM_write(addr32, value);
    } else if (((addr32) >= 0xB8000UL) && ((addr32) < 0xBC000UL)) {             // CGA video RAM range
        addr32 -= 0xB8000UL;
        if ((videomode == 0) || (videomode == 1) || (videomode == 2) || (videomode == 3)) {
            screenmem[addr32 & 4095] = value;                                           // 4k if we are in text mode
        } else {
            screenmem[addr32 & 16383] = value;                                          // 16k for graphic mode!!!
        }
    } else if (((addr32) >= 0xD0000UL) && ((addr32) < 0xD8000UL)) {
        addr32 -= 0xCC000UL;
    } else if ((addr32) > 0xFFFFFUL) {
        SRAM_write(addr32, value);
    }
}

#define writew86(addr32, value) {write86((addr32),(uint8_t)(value));write86((addr32)+1,(uint8_t)((uint16_t)(value)>>8));}

uint8_t __inline SRAM_read(uint32_t addr32) {
    return RAM[addr32];
}

uint8_t read86(uint32_t addr32) {
    if (addr32 < SRAM_SIZE) {
        // https://docs.huihoo.com/gnu_linux/own_os/appendix-bios_memory_2.htm

        switch (addr32) { //some hardcoded values for the BIOS data area

            case 0x400:     // serial COM1: address at 0x03F8
                return (0xF8);
            case 0x401:
                return (0x03);

            case 0x402:
            case 0x403:
                return (0x00);

            case 0x410:
                return (0x61); //video type CGA 80x25

            case 0x411:
                return (0x02); // WS: one serial port

            case 0x413:
                return (0x80);
            case 0x414:
                return (0x02);

            case 0x463:
                return (0x3d4);

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

            case 0x466:
                return pr3D9;

            case 0x475:                //hard drive count
                return (hdcount);

            default:
                return SRAM_read(addr32);
        }
    } else if ((addr32 >= 0xFE000UL) && (addr32 <= 0xFFFFFUL)) {                              // BIOS ROM range
        addr32 -= 0xFE000UL;
        return BIOS[addr32];
    } else if ((addr32 >= 0xB8000UL) && (addr32 < 0xBC000UL)) {                               // CGA video RAM range
        addr32 -= 0xB8000UL;
        if ((videomode == 0) || (videomode == 1) || (videomode == 2) || (videomode == 3)) {
            return screenmem[addr32 & 4095];
        } else {
            return screenmem[addr32 & 16383];                                                       //16k CGA MEMORY!!!
        }
    } else if ((addr32 >= 0xD0000UL) && (addr32 < 0xD8000UL)) {
        addr32 -= 0xCC000UL;
    } else if ((addr32 >= 0xF6000UL) && (addr32 < 0xFA000UL)) {                               // IBM BASIC ROM LOW
        addr32 -= 0xF6000UL;
        //return BASICL[addr32];
    } else if ((addr32 >= 0xFA000UL) && (addr32 < 0xFE000UL)) {                               // IBM BASIC ROM HIGH
        addr32 -= 0xFA000UL;
        //return BASICH[addr32];
    } else if ((addr32) > 0xFFFFFUL) {
        return SRAM_read(addr32);
    }
}

#define readw86(addr32) ((uint16_t)read86((addr32))|((uint16_t)read86((addr32)+1)<<8))

inline void flag_szp8(uint8_t value) __attribute__((always_inline));

void flag_szp8(uint8_t value) {
    if (!(value)) zf = 1; else zf = 0;
    if ((value) & 0x80) sf = 1; else sf = 0;
    pf = parity[value];
}

inline void flag_szp16(uint16_t value) __attribute__((always_inline));

void flag_szp16(uint16_t value) {
    if (!(value)) zf = 1; else zf = 0;
    if (value & 0x8000) sf = 1; else sf = 0;
    pf = parity[(uint8_t) value];
}

inline void flag_log8(uint8_t value) __attribute__((always_inline));

void flag_log8(uint8_t value) {
    flag_szp8(value);
    cf = 0;
    of = 0;
}

inline void flag_log16(uint16_t value) __attribute__((always_inline));

void flag_log16(uint16_t value) {
    flag_szp16(value);
    cf = 0;
    of = 0;
}

inline void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3) __attribute__((always_inline));

void flag_adc8(uint8_t v1, uint8_t v2, uint8_t v3) { //v1 = destination operand, v2 = source operand, v3 = carry flag
    uint16_t dst;
    dst = (uint16_t) (v1) + (uint16_t) (v2) + (uint16_t) (v3);
    flag_szp8((uint8_t) dst);
    if (((dst ^ (v1)) & (dst ^ (v2)) & 0x80) == 0x80) of = 1; else of = 0;
    if (dst & 0xFF00) cf = 1; else cf = 0;
    if ((((v1) ^ (v2) ^ dst) & 0x10) == 0x10) af = 1; else af = 0;
}

inline void flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3) __attribute__((always_inline));

void
flag_adc16(uint16_t v1, uint16_t v2, uint16_t v3) { //v1 = destination operand, v2 = source operand, v3 = carry flag
    uint32_t dst;
    dst = (uint32_t) (v1) + (uint32_t) (v2) + (uint32_t) (v3);
    flag_szp16((uint16_t) dst);
    if ((((dst ^ (v1)) & (dst ^ (v2))) & 0x8000) == 0x8000) of = 1; else of = 0;
    if (dst & 0xFFFF0000UL) cf = 1; else cf = 0;
    if ((((v1) ^ (v2) ^ dst) & 0x10) == 0x10) af = 1; else af = 0;
}

inline void flag_add8(uint8_t v1, uint8_t v2) __attribute__((always_inline));

void flag_add8(uint8_t v1, uint8_t v2) { //v1 = destination operand, v2 = source operand
    uint16_t dst;
    dst = (uint16_t) (v1) + (uint16_t) (v2);
    flag_szp8((uint8_t) dst);
    if (dst & 0xFF00) cf = 1; else cf = 0;
    if (((dst ^ (v1)) & (dst ^ (v2)) & 0x80) == 0x80) of = 1; else of = 0;
    if ((((v1) ^ (v2) ^ dst) & 0x10) == 0x10) af = 1; else af = 0;
}

inline void flag_add16(uint16_t v1, uint16_t v2) __attribute__((always_inline));

void flag_add16(uint16_t v1, uint16_t v2) { //v1 = destination operand, v2 = source operand
    uint32_t dst;
    dst = (uint32_t) (v1) + (uint32_t) (v2);
    flag_szp16((uint16_t) dst);
    if (dst & 0xFFFF0000UL) cf = 1; else cf = 0;
    if (((dst ^ (v1)) & (dst ^ (v2)) & 0x8000) == 0x8000) of = 1; else of = 0;
    if ((((v1) ^ (v2) ^ dst) & 0x10) == 0x10) af = 1; else af = 0;
}


inline void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3) __attribute__((always_inline));

void flag_sbb8(uint8_t v1, uint8_t v2, uint8_t v3) { //v1 = destination operand, v2 = source operand, v3 = carry flag
    uint16_t dst;
    uint16_t newv2;
    newv2 = (uint16_t) (v2) + (uint16_t) (v3);
    dst = (uint16_t) (v1) - (uint16_t) newv2;
    flag_szp8((uint8_t) dst);
    if (dst & 0xFF00) cf = 1; else cf = 0;
    if ((dst ^ (v1)) & ((v1) ^ newv2) & 0x80) of = 1; else of = 0;
    if (((v1) ^ newv2 ^ dst) & 0x10) af = 1; else af = 0;
}

inline void flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3) __attribute__((always_inline));

void
flag_sbb16(uint16_t v1, uint16_t v2, uint16_t v3) { //v1 = destination operand, v2 = source operand, v3 = carry flag
    uint32_t dst;
    uint32_t newv2;
    newv2 = (uint32_t) (v2) + (uint32_t) (v3);
    dst = (uint32_t) v1 - newv2;
    flag_szp16((uint16_t) dst);
    if (dst & 0xFFFF0000UL) cf = 1; else cf = 0;
    if ((dst ^ (v1)) & (v1 ^ newv2) & 0x8000) of = 1; else of = 0;
    if (((v1) ^ newv2 ^ dst) & 0x10) af = 1; else af = 0;
}

inline void flag_sub8(uint8_t v1, uint8_t v2) __attribute__((always_inline));

void flag_sub8(uint8_t v1, uint8_t v2) { //v1 = destination operand, v2 = source operand
    uint16_t dst;
    dst = (uint16_t) (v1) - (uint16_t) (v2);
    flag_szp8((uint8_t) dst);
    if (dst & 0xFF00) cf = 1; else cf = 0;
    if ((dst ^ (v1)) & ((v1) ^ (v2)) & 0x80) of = 1; else of = 0;
    if (((v1) ^ (v2) ^ dst) & 0x10) af = 1; else af = 0;
}

inline void flag_sub16(uint16_t v1, uint16_t v2) __attribute__((always_inline));

void flag_sub16(uint16_t v1, uint16_t v2) { //v1 = destination operand, v2 = source operand
    uint32_t dst;
    dst = (uint32_t) (v1) - (uint32_t) (v2);
    flag_szp16((uint16_t) dst);
    if (dst & 0xFFFF0000UL) cf = 1; else cf = 0;
    if ((dst ^ (v1)) & ((v1) ^ (v2)) & 0x8000) of = 1; else of = 0;
    if (((v1) ^ (v2) ^ dst) & 0x10) af = 1; else af = 0;
}

inline void op_adc8() __attribute__((always_inline));

void op_adc8() {
    res8 = oper1b + oper2b + cf;
    flag_adc8(oper1b, oper2b, cf);
}

inline void op_adc16() __attribute__((always_inline));

void op_adc16() {
    res16 = oper1 + oper2 + cf;
    flag_adc16(oper1, oper2, cf);
}

inline void op_add8() __attribute__((always_inline));

void op_add8() {
    res8 = oper1b + oper2b;
    flag_add8(oper1b, oper2b);
}

inline void op_add16() __attribute__((always_inline));

void op_add16() {
    res16 = oper1 + oper2;
    flag_add16(oper1, oper2);
}

inline void op_and8() __attribute__((always_inline));

void op_and8() {
    res8 = oper1b & oper2b;
    flag_log8(res8);
}

inline void op_and16() __attribute__((always_inline));

void op_and16() {
    res16 = oper1 & oper2;
    flag_log16(res16);
}

inline void op_or8() __attribute__((always_inline));

void op_or8() {
    res8 = oper1b | oper2b;
    flag_log8(res8);
}

inline void op_or16() __attribute__((always_inline));

void op_or16() {
    res16 = oper1 | oper2;
    flag_log16(res16);
}

inline void op_xor8() __attribute__((always_inline));

void op_xor8() {
    res8 = oper1b ^ oper2b;
    flag_log8(res8);
}

inline void op_xor16() __attribute__((always_inline));

void op_xor16() {
    res16 = oper1 ^ oper2;
    flag_log16(res16);
}

inline void op_sub8() __attribute__((always_inline));

void op_sub8() {
    res8 = oper1b - oper2b;
    flag_sub8(oper1b, oper2b);
}

inline void op_sub16() __attribute__((always_inline));

void op_sub16() {
    res16 = oper1 - oper2;
    flag_sub16(oper1, oper2);
}

inline void op_sbb8() __attribute__((always_inline));

void op_sbb8() {
    res8 = oper1b - (oper2b + cf);
    flag_sbb8(oper1b, oper2b, cf);
}

inline void op_sbb16() __attribute__((always_inline));

void op_sbb16() {
    res16 = oper1 - (oper2 + cf);
    flag_sbb16(oper1, oper2, cf);
}

inline void getea(uint8_t rmval) __attribute__((always_inline));

void getea(uint8_t rmval) {
    uint32_t tempea;
    tempea = 0;
    switch (mode) {
        case 0:
            switch (rmval) {
                case 0:
                    tempea = regs.wordregs[regbx] + regs.wordregs[regsi];
                    break;
                case 1:
                    tempea = regs.wordregs[regbx] + regs.wordregs[regdi];
                    break;
                case 2:
                    tempea = regs.wordregs[regbp] + regs.wordregs[regsi];
                    break;
                case 3:
                    tempea = regs.wordregs[regbp] + regs.wordregs[regdi];
                    break;
                case 4:
                    tempea = regs.wordregs[regsi];
                    break;
                case 5:
                    tempea = regs.wordregs[regdi];
                    break;
                case 6:
                    tempea = disp16;
                    break;
                case 7:
                    tempea = regs.wordregs[regbx];
                    break;
            }
            break;
        case 1:
        case 2:
            switch (rmval) {
                case 0:
                    tempea = regs.wordregs[regbx] + regs.wordregs[regsi] + disp16;
                    break;
                case 1:
                    tempea = regs.wordregs[regbx] + regs.wordregs[regdi] + disp16;
                    break;
                case 2:
                    tempea = regs.wordregs[regbp] + regs.wordregs[regsi] + disp16;
                    break;
                case 3:
                    tempea = regs.wordregs[regbp] + regs.wordregs[regdi] + disp16;
                    break;
                case 4:
                    tempea = regs.wordregs[regsi] + disp16;
                    break;
                case 5:
                    tempea = regs.wordregs[regdi] + disp16;
                    break;
                case 6:
                    tempea = regs.wordregs[regbp] + disp16;
                    break;
                case 7:
                    tempea = regs.wordregs[regbx] + disp16;
                    break;
            }
            break;
    }
    ea = useseg;
    ea <<= 4;
    ea += (tempea & 0xFFFF);
}

inline void modregrm() __attribute__((always_inline));

void modregrm() {
    addrbyte = getmem8(segregs[regcs], ip);
    StepIP(1);
    mode = addrbyte >> 6;
    reg = (addrbyte >> 3) & 7;
    rm = addrbyte & 7;
    switch (mode) {
        case 0:
            if (rm == 6) {
                disp16 = getmem16(segregs[regcs], ip);
                StepIP(2);
            }
            if (((rm == 2) || (rm == 3)) && !segoverride) useseg = segregs[regss];
            break;
        case 1:
            disp16 = signext(getmem8(segregs[regcs], ip));
            StepIP(1);
            if (((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) useseg = segregs[regss];
            break;
        case 2:
            disp16 = getmem16(segregs[regcs], ip);
            StepIP(2);
            if (((rm == 2) || (rm == 3) || (rm == 6)) && !segoverride) useseg = segregs[regss];
            break;
        default:
            disp8 = 0;
            disp16 = 0;
    }
    if (mode < 3) getea(rm);
}

inline void push(uint16_t pushval) __attribute__((always_inline));

void push(uint16_t pushval) {
    putreg16(regsp, getreg16(regsp) - 2);
    putmem16(segregs[regss], getreg16(regsp), pushval);
}

inline uint16_t pop()  __attribute__((always_inline));

uint16_t pop() {
    uint16_t tempval;
    tempval = getmem16(segregs[regss], getreg16(regsp));
    putreg16(regsp, getreg16(regsp) + 2);
    return (tempval);
}

void reset86() {
    uint16_t i, cnt, bitcount;
    segregs[regcs] = 0xFFFF;
    ip = 0x0000;
    segregs[regss] = 0x0000;
    regs.wordregs[regsp] = 0xFFFE;

    //generate parity lookup table
    for (i = 0; i < 256; i++) {
        bitcount = 0;
        for (cnt = 0; cnt < 8; cnt++)
            bitcount += ((i >> cnt) & 1);
        if (bitcount & 1) parity[i] = 0; else parity[i] = 1;
    }
}


inline uint16_t readrm16(uint8_t rmval) __attribute__((always_inline));

uint16_t readrm16(uint8_t rmval) {
    if (mode < 3) {
        getea(rmval);
        return (read86(ea) | ((uint16_t) read86(ea + 1) << 8));
    } else {
        return (getreg16(rmval));
    }
}

inline uint8_t readrm8(uint8_t rmval) __attribute__((always_inline));

uint8_t readrm8(uint8_t rmval) {
    if (mode < 3) {
        //getea(rmval);
        return (read86(ea));
    } else {
        return (getreg8(rmval));
    }
}

inline void writerm16(uint8_t rmval, uint16_t value) __attribute__((always_inline));

void writerm16(uint8_t rmval, uint16_t value) {
    if (mode < 3) {
        write86(ea, value & 0xFF);
        write86(ea + 1, value >> 8);
    } else {
        putreg16(rmval, value);
    }
}

inline void writerm8(uint8_t rmval, uint8_t value) __attribute__((always_inline));

void writerm8(uint8_t rmval, uint8_t value) {
    if (mode < 3) {
        write86(ea, value);
    } else {
        putreg8(rmval, value);
    }
}

inline uint8_t op_grp2_8(uint8_t cnt) __attribute__((always_inline));

uint8_t op_grp2_8(uint8_t cnt) {
    uint16_t s, oldcf, msb;
    uint8_t shift;
    s = oper1b;
    oldcf = cf;
    switch (reg) {
        case 0: //ROL r/m8
            for (shift = 1; shift <= cnt; shift++) {
                if (s & 0x80) cf = 1; else cf = 0;
                s = s << 1;
                s = s | cf;
            }
            if (cnt == 1) of = cf ^ ((s >> 7) & 1);
            break;

        case 1: //ROR r/m8
            for (shift = 1; shift <= cnt; shift++) {
                cf = s & 1;
                s = (s >> 1) | (cf << 7);
            }
            if (cnt == 1) of = (s >> 7) ^ ((s >> 6) & 1);
            break;

        case 2: //RCL r/m8
            for (shift = 1; shift <= cnt; shift++) {
                oldcf = cf;
                if (s & 0x80) cf = 1; else cf = 0;
                s = s << 1;
                s = s | oldcf;
            }
            if (cnt == 1) of = cf ^ ((s >> 7) & 1);
            break;

        case 3: //RCR r/m8
            for (shift = 1; shift <= cnt; shift++) {
                oldcf = cf;
                cf = s & 1;
                s = (s >> 1) | (oldcf << 7);
            }
            if (cnt == 1) of = (s >> 7) ^ ((s >> 6) & 1);
            break;

        case 4:
        case 6: //SHL r/m8
            for (shift = 1; shift <= cnt; shift++) {
                if (s & 0x80) cf = 1; else cf = 0;
                s = (s << 1) & 0xFF;
            }
            if ((cnt == 1) && (cf == (s >> 7))) of = 0; else of = 1;
            flag_szp8((uint8_t) s);
            break;

        case 5: //SHR r/m8
            if ((cnt == 1) && (s & 0x80)) of = 1;
            else of = 0;
            for (shift = 1; shift <= cnt; shift++) {
                cf = s & 1;
                s = s >> 1;
            }
            flag_szp8((uint8_t) s);
            break;

        case 7: //SAR r/m8
            for (shift = 1; shift <= cnt; shift++) {
                msb = s & 0x80;
                cf = s & 1;
                s = (s >> 1) | msb;
            }
            of = 0;
            flag_szp8((uint8_t) s);
            break;

    }
    return (s & 0xFF);
}

inline uint16_t op_grp2_16(uint8_t cnt) __attribute__((always_inline));

uint16_t op_grp2_16(uint8_t cnt) {
    uint32_t s, oldcf, msb;
    uint8_t shift;
    s = oper1;
    oldcf = cf;
    switch (reg) {
        case 0: //ROL r/m8
            for (shift = 1; shift <= cnt; shift++) {
                if (s & 0x8000) cf = 1; else cf = 0;
                s = s << 1;
                s = s | cf;
            }
            if (cnt == 1) of = cf ^ ((s >> 15) & 1);
            break;

        case 1: //ROR r/m8
            for (shift = 1; shift <= cnt; shift++) {
                cf = s & 1;
                s = (s >> 1) | (cf << 15);
            }
            if (cnt == 1) of = (s >> 15) ^ ((s >> 14) & 1);
            break;

        case 2: //RCL r/m8
            for (shift = 1; shift <= cnt; shift++) {
                oldcf = cf;
                if (s & 0x8000) cf = 1; else cf = 0;
                s = s << 1;
                s = s | oldcf;
            }
            if (cnt == 1) of = cf ^ ((s >> 15) & 1);
            break;

        case 3: //RCR r/m8
            for (shift = 1; shift <= cnt; shift++) {
                oldcf = cf;
                cf = s & 1;
                s = (s >> 1) | (oldcf << 15);
            }
            if (cnt == 1) of = (s >> 15) ^ ((s >> 14) & 1);
            break;

        case 4:
        case 6: //SHL r/m8
            for (shift = 1; shift <= cnt; shift++) {
                if (s & 0x8000) cf = 1; else cf = 0;
                s = (uint16_t) (s << 1);
            }
            if ((cnt == 1) && (cf == (s >> 15))) of = 0; else of = 1;
            flag_szp16((uint16_t) s);
            break;

        case 5: //SHR r/m8
            if ((cnt == 1) && (s & 0x8000)) of = 1;
            else of = 0;
            for (shift = 1; shift <= cnt; shift++) {
                cf = s & 1;
                s = s >> 1;
            }
            flag_szp16((uint16_t) s);
            break;

        case 7: //SAR r/m8
            for (shift = 1; shift <= cnt; shift++) {
                msb = s & 0x8000;
                cf = s & 1;
                s = (s >> 1) | msb;
            }
            of = 0;
            flag_szp16((uint16_t) s);
            break;

    }
    return ((uint16_t) s);
}

inline void op_div8(uint16_t valdiv, uint8_t divisor) __attribute__((always_inline));

void op_div8(uint16_t valdiv, uint8_t divisor) {
    if (divisor == 0) {
        intcall86(0);
        return;
    }
    if ((valdiv / (uint16_t) divisor) > 0xFF) {
        intcall86(0);
        return;
    }
    regs.byteregs[regah] = valdiv % (uint16_t) divisor;
    regs.byteregs[regal] = valdiv / (uint16_t) divisor;
}


inline void op_idiv8(uint16_t valdiv, uint8_t divisor) __attribute__((always_inline));

void op_idiv8(uint16_t valdiv, uint8_t divisor) {
    uint16_t s1, s2, d1, d2, sign;
    if (divisor == 0) {
        intcall86(0);
        return;
    }
    s1 = valdiv;
    s2 = divisor;
    sign = (((s1 ^ s2) & 0x8000) != 0);
    s1 = (s1 < 0x8000) ? s1 : (uint16_t) (~s1 + 1);
    s2 = (s2 < 0x8000) ? s2 : (uint16_t) (~s2 + 1);
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
    regs.byteregs[regah] = d2;
    regs.byteregs[regal] = d1;
}

inline void op_grp3_8() __attribute__((always_inline));

void op_grp3_8() {
    oper1 = signext(oper1b);
    oper2 = signext(oper2b);
    switch (reg) {
        case 0:
        case 1: //TEST
            flag_log8(oper1b & getmem8(segregs[regcs], ip));
            StepIP(1);
            break;

        case 2: //NOT
            res8 = ~oper1b;
            break;

        case 3: //NEG
            res8 = (~oper1b) + 1;
            flag_sub8(0, oper1b);
            if (res8 == 0) cf = 0; else cf = 1;
            break;

        case 4: //MUL
            temp1 = (uint32_t) oper1b * (uint32_t) regs.byteregs[regal];
            putreg16(regax, (uint16_t) temp1);
            flag_szp8((uint8_t) temp1);
            if (regs.byteregs[regah]) {
                cf = 1;
                of = 1;
            } else {
                cf = 0;
                of = 0;
            }
            break;

        case 5: //IMUL
            oper1 = signext(oper1b);
            temp1 = signext(regs.byteregs[regal]);
            temp2 = oper1;
            if ((temp1 & 0x80) == 0x80) temp1 = temp1 | 0xFFFFFF00UL;
            if ((temp2 & 0x80) == 0x80) temp2 = temp2 | 0xFFFFFF00UL;
            temp3 = (uint16_t) (temp1 * temp2);
            putreg16(regax, (uint16_t) temp3);
            if (regs.byteregs[regah]) {
                cf = 1;
                of = 1;
            } else {
                cf = 0;
                of = 0;
            }
            break;

        case 6: //DIV
            op_div8(getreg16(regax), oper1b);
            break;

        case 7: //IDIV
            op_idiv8(getreg16(regax), oper1b);
            break;
    }
}

void op_div16(uint32_t valdiv, uint16_t divisor) {
    if (divisor == 0) {
        intcall86(0);
        return;
    }
    if ((valdiv / (uint32_t) divisor) > 0xFFFF) {
        intcall86(0);
        return;
    }
    putreg16(regdx, valdiv % (uint32_t) divisor);
    putreg16(regax, valdiv / (uint32_t) divisor);
}

void op_idiv16(uint32_t valdiv, uint16_t divisor) {
    uint32_t d1, d2, s1, s2, sign;
    if (divisor == 0) {
        intcall86(0);
        return;
    }
    s1 = valdiv;
    s2 = divisor;
    s2 = (s2 & 0x8000) ? (s2 | 0xffff0000UL) : s2;
    sign = (((s1 ^ s2) & 0x80000000UL) != 0);
    s1 = (s1 < 0x80000000UL) ? s1 : ((~s1 + 1) & 0xffffffffUL);
    s2 = (s2 < 0x80000000UL) ? s2 : ((~s2 + 1) & 0xffffffffUL);
    d1 = s1 / s2;
    d2 = s1 % s2;
    if (d1 & 0xFFFF0000UL) {
        intcall86(0);
        return;
    }
    if (sign) {
        d1 = (uint16_t) (~d1 + 1);
        d2 = (uint16_t) (~d2 + 1);
    }
    putreg16(regax, d1);
    putreg16(regdx, d2);
}

inline void op_grp3_16() __attribute__((always_inline));

void op_grp3_16() {
    switch (reg) {
        case 0:
        case 1: //TEST
            flag_log16(oper1 & getmem16(segregs[regcs], ip));
            StepIP(2);
            break;
        case 2: //NOT
            res16 = ~oper1;
            break;
        case 3: //NEG
            res16 = (~oper1) + 1;
            flag_sub16(0, oper1);
            if (res16) cf = 1; else cf = 0;
            break;
        case 4: //MUL
            temp1 = (uint32_t) oper1 * (uint32_t) getreg16(regax);
            putreg16(regax, (uint16_t) temp1);
            putreg16(regdx, temp1 >> 16);
            flag_szp16((uint16_t) temp1);
            if (getreg16(regdx)) {
                cf = 1;
                of = 1;
            } else {
                cf = 0;
                of = 0;
            }
            break;
        case 5: //IMUL
            temp1 = getreg16(regax);
            temp2 = oper1;
            if (temp1 & 0x8000) temp1 |= 0xFFFF0000UL;
            if (temp2 & 0x8000) temp2 |= 0xFFFF0000UL;
            temp3 = temp1 * temp2;
            putreg16(regax, (uint16_t) temp3); //into register ax
            putreg16(regdx, temp3 >> 16); //into register dx
            if (getreg16(regdx)) {
                cf = 1;
                of = 1;
            } else {
                cf = 0;
                of = 0;
            }
            break;
        case 6: //DIV
            op_div16(((uint32_t) getreg16(regdx) << 16) + (uint32_t) getreg16(regax), oper1);
            break;
        case 7: //DIV
            op_idiv16(((uint32_t) getreg16(regdx) << 16) + (uint32_t) getreg16(regax), oper1);
            break;
    }
}

inline void op_grp5() __attribute__((always_inline));

void op_grp5() {
    switch (reg) {
        case 0: /*INC Ev*/
            oper2 = 1;
            tempcf = cf;
            op_add16();
            cf = tempcf;
            writerm16(rm, res16);
            break;
        case 1: /*DEC Ev*/
            oper2 = 1;
            tempcf = cf;
            op_sub16();
            cf = tempcf;
            writerm16(rm, res16);
            break;
        case 2: /*CALL Ev*/
            push(ip);
            ip = oper1;
            break;
        case 3: /*CALL Mp*/
            push(segregs[regcs]);
            push(ip);
            /*getea(rm);*/
            ip = (uint16_t) read86(ea) + ((uint16_t) read86(ea + 1) << 8);
            segregs[regcs] = (uint16_t) read86(ea + 2) + ((uint16_t) read86(ea + 3) << 8);
            break;
        case 4: /*JMP Ev*/
            ip = oper1;
            break;
        case 5: /*JMP Mp*/
            /*getea(rm);*/
            ip = (uint16_t) read86(ea) + ((uint16_t) read86(ea + 1) << 8);
            segregs[regcs] = (uint16_t) read86(ea + 2) + ((uint16_t) read86(ea + 3) << 8);
            break;
        case 6: /*PUSH Ev*/
            push(oper1);
            break;
    }
}

uint8_t didintr = 0;

//======================================================================== BIOS INTERRUPT SERVICES ========================================================================

void intcall86(uint8_t intnum) {
    didintr = 1;
    //printf("INT 0x%x\r\n", intnum);
    switch (intnum) {
        case 0x09:    // keyboard interrupt
            break;

        case 0x10:    // video services
            //printf("VIDEO MODE\r\n");
            switch (regs.byteregs[regah]) {
                case 0x00:          // change video mode
                    if (regs.byteregs[regal] == 0x00) {
                        videomode = 0;                      // 40x25 text 16 grey
                        CopyCharROM();                   // move charROM back to RAM
                    } else if (regs.byteregs[regal] == 0x01) {
                        videomode = 1;                     // 40x25 text 16 fore/8 back
                        CopyCharROM();                  // move charROM back to RAM
                    } else if (regs.byteregs[regal] == 0x02) {
                        videomode = 2;                     // 80x25 text 16 grey
                        CopyCharROM();                  // move charROM back to RAM
                    } else if (regs.byteregs[regal] == 0x03) {
                        videomode = 3;                    // 80x25 text 16 fore/8 back
                        CopyCharROM();                 // move charROM back to RAM
                    } else if (regs.byteregs[regal] == 0x04)
                        videomode = 4;                 // 320x200 graphics 4 colours
                    else if (regs.byteregs[regal] == 0x05) videomode = 5;                 // 320x200 graphics 4 grey
                    else if (regs.byteregs[regal] == 0x06) videomode = 6;                 // 640x200 graphics 2 colours
                    break;

                case 0x01:                            // show cursor
                    if (regs.byteregs[regch] == 0x32) {
                        curshow = false;
                        break;
                    } else if ((regs.byteregs[regch] == 0x06) && (regs.byteregs[regcl] == 0x07)) {
                        CURSOR_ASCII = 95;
                        curshow = true;
                        break;
                    } else if ((regs.byteregs[regch] == 0x00) && (regs.byteregs[regcl] == 0x07)) {
                        CURSOR_ASCII = 219;
                        curshow = true;
                    }
                    return;

                case 0x02: // Установить позицию курсора
                    cursor_x = CPU_DL;
                    cursor_y = CPU_DH;
                    return;
                case 0x03: // Получить позицию курсора
                    CPU_DL = cursor_x;
                    CPU_DH = cursor_y;
                    return;

                case 0x08: // Получим чар под курсором
                    CPU_AL = screenmem[(cursor_y * 160 + cursor_x * 2) + 0];
                    CPU_AH = screenmem[(cursor_y * 160 + cursor_x * 2) + 1];
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
                    bios_putchar(CPU_AL);
                    return;
                default:
                    break;
            }
            break;

        case 0x13: //disk services
            diskhandler();
            return;

        case 0x14:  // serial I/O services, added by WS
            break;

        case 0x16: //keyboard services
            break;

        case 0x19: //bootstrap
            if (bootdrive < 255) {                //read first sector of boot drive into 07C0:0000 and execute it
                regs.byteregs[regdl] = bootdrive;
                bios_read_boot_sector((bootdrive & 0x80) ? bootdrive - 126 : bootdrive, 0x07C0, 0x0000);
                segregs[regcs] = 0x0000;
                ip = 0x7C00;
            } else {
                segregs[regcs] = 0xF600;  //start ROM BASIC at bootstrap if requested
                ip = 0x0000;
            }
            return;

        case 0x1A:                              // System Timer and Clock Services
            switch (regs.byteregs[regah]) {
                case 0x01:                          // Set time
                    tempdummy = regs.byteregs[regdl] | (regs.byteregs[regdh] << 8) | (regs.byteregs[regcl] << 16) |
                                (regs.byteregs[regch] << 24);
                    tickNumber = tempdummy;
                    break;

                default:
                    break;
            }

        default:
            break;

    }

    push(makeflagsword());
    push(segregs[regcs]);
    push(ip);
    segregs[regcs] = getmem16(0, ((uint16_t) intnum << 2) + 2);
    ip = getmem16(0, ((uint16_t) intnum << 2));
    ifl = 0;
    tf = 0;
}

// ================================================================================= EXEC86 ========================================================================

uint64_t frametimer = 0, didwhen = 0, didticks = 0;
uint32_t makeupticks = 0;
extern float timercomp;
uint64_t timerticks = 0, realticks = 0;
uint64_t lastcountertimer = 0, counterticks = 10000;
uint8_t hltstate = 0;
uint8_t running = 0, didbootstrap = 0;

void exec86(uint32_t execloops) {
    uint8_t docontinue;
    static uint16_t firstip;
    static uint16_t trap_toggle = 0;

    //counterticks = (uint64_t) ( (double) timerfreq / (double) 65536.0);

    for (uint32_t loopcount = 0; loopcount < execloops; loopcount++) {
        if (trap_toggle) {
            intcall86(1);
        }

        if (tf) {
            trap_toggle = 1;
        } else {
            trap_toggle = 0;
        }

        if (!trap_toggle && (ifl && (i8259.irr & (~i8259.imr)))) {
            hltstate = 0;
            intcall86(nextintr()); // get next interrupt from the i8259, if any d
        }

        if (hltstate) {
            //puts("CPU:d HALTED!!!");
            goto skipexecution;
        }

        reptype = 0;
        segoverride = 0;
        useseg = segregs[regds];
        docontinue = 0;
        firstip = ip;

        if ((segregs[regcs] == 0xF000) && (ip == 0xE066)) {
            printf("didbootsreap\r\n");
            didbootstrap = 0; //detect if we hit the BIOS entry point to clear didbootstrap because we've rebooted
        }

        while (!docontinue) {
            segregs[regcs] = segregs[regcs] & 0xFFFF;
            ip = ip & 0xFFFF;
            savecs = segregs[regcs];
            saveip = ip;
            opcode = getmem8 (segregs[regcs], ip);
            StepIP (1);

#ifdef DEBUG
            Serial.print("op:");
          Serial.print(opcode, HEX);
          Serial.print(" ip:");
          Serial.print(ip, HEX);
          Serial.print(" ax:");
          Serial.print(regs.wordregs[regax], HEX);
          Serial.print(" bx:");
          Serial.print(regs.wordregs[regbx], HEX);
          Serial.print(" cx:");
          Serial.print(regs.wordregs[regcx],  HEX);
          Serial.print(" dx:");
          Serial.println(regs.wordregs[regdx], HEX);
#endif

            switch (opcode) {
                /* segment prefix check */
                case 0x2E:  /* segment segregs[regcs] */
                    useseg = segregs[regcs];
                    segoverride = 1;
                    break;

                case 0x3E:  /* segment segregs[regds] */
                    useseg = segregs[regds];
                    segoverride = 1;
                    break;

                case 0x26:  /* segment segregs[reges] */
                    useseg = segregs[reges];
                    segoverride = 1;
                    break;

                case 0x36:  /* segment segregs[regss] */
                    useseg = segregs[regss];
                    segoverride = 1;
                    break;

                    /* repetition prefix check */
                case 0xF3:  /* REP/REPE/REPZ */
                    reptype = 1;
                    break;

                case 0xF2:  /* REPNE/REPNZ */
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
                oper2b = getreg8 (reg);
                op_add8();
                writerm8(rm, res8);
                break;

            case 0x1: /* 01 ADD Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16 (reg);
                op_add16();
                writerm16(rm, res16);
                break;

            case 0x2: /* 02 ADD Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                op_add8();
                putreg8 (reg, res8);
                break;

            case 0x3: /* 03 ADD Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                op_add16();
                putreg16 (reg, res16);
                break;

            case 0x4: /* 04 ADD regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                op_add8();
                regs.byteregs[regal] = res8;
                break;

            case 0x5: /* 05 ADD eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                op_add16();
                regs.wordregs[regax] = res16;
                break;

            case 0x6: /* 06 PUSH segregs[reges] */
                push(segregs[reges]);
                break;

            case 0x7: /* 07 POP segregs[reges] */
                segregs[reges] = pop();
                break;

            case 0x8: /* 08 OR Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8 (reg);
                op_or8();
                writerm8(rm, res8);
                break;

            case 0x9: /* 09 OR Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16 (reg);
                op_or16();
                writerm16(rm, res16);
                break;

            case 0xA: /* 0A OR Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                op_or8();
                putreg8 (reg, res8);
                break;

            case 0xB: /* 0B OR Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                op_or16();
                if ((oper1 == 0xF802) && (oper2 == 0xF802)) {
                    sf = 0; /* cheap hack to make Wolf 3D think we're a 286 so it plays */
                }

                putreg16 (reg, res16);
                break;

            case 0xC: /* 0C OR regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                op_or8();
                regs.byteregs[regal] = res8;
                break;

            case 0xD: /* 0D OR eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                op_or16();
                regs.wordregs[regax] = res16;
                break;

            case 0xE: /* 0E PUSH segregs[regcs] */
                push(segregs[regcs]);
                break;

            case 0xF: //0F POP CS only the 8086/8088 does this.
                segregs[regcs] = pop();
                break;

            case 0x10:  /* 10 ADC Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8 (reg);
                op_adc8();
                writerm8(rm, res8);
                break;

            case 0x11:  /* 11 ADC Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16 (reg);
                op_adc16();
                writerm16(rm, res16);
                break;

            case 0x12:  /* 12 ADC Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                op_adc8();
                putreg8 (reg, res8);
                break;

            case 0x13:  /* 13 ADC Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                op_adc16();
                putreg16 (reg, res16);
                break;

            case 0x14:  /* 14 ADC regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                op_adc8();
                regs.byteregs[regal] = res8;
                break;

            case 0x15:  /* 15 ADC eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                op_adc16();
                regs.wordregs[regax] = res16;
                break;

            case 0x16:  /* 16 PUSH segregs[regss] */
                push(segregs[regss]);
                break;

            case 0x17:  /* 17 POP segregs[regss] */
                segregs[regss] = pop();
                break;

            case 0x18:  /* 18 SBB Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8 (reg);
                op_sbb8();
                writerm8(rm, res8);
                break;

            case 0x19:  /* 19 SBB Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16 (reg);
                op_sbb16();
                writerm16(rm, res16);
                break;

            case 0x1A:  /* 1A SBB Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                op_sbb8();
                putreg8 (reg, res8);
                break;

            case 0x1B:  /* 1B SBB Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                op_sbb16();
                putreg16 (reg, res16);
                break;

            case 0x1C:  /* 1C SBB regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                op_sbb8();
                regs.byteregs[regal] = res8;
                break;

            case 0x1D:  /* 1D SBB eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                op_sbb16();
                regs.wordregs[regax] = res16;
                break;

            case 0x1E:  /* 1E PUSH segregs[regds] */
                push(segregs[regds]);
                break;

            case 0x1F:  /* 1F POP segregs[regds] */
                segregs[regds] = pop();
                break;

            case 0x20:  /* 20 AND Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8 (reg);
                op_and8();
                writerm8(rm, res8);
                break;

            case 0x21:  /* 21 AND Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16 (reg);
                op_and16();
                writerm16(rm, res16);
                break;

            case 0x22:  /* 22 AND Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                op_and8();
                putreg8 (reg, res8);
                break;

            case 0x23:  /* 23 AND Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                op_and16();
                putreg16 (reg, res16);
                break;

            case 0x24:  /* 24 AND regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                op_and8();
                regs.byteregs[regal] = res8;
                break;

            case 0x25:  /* 25 AND eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                op_and16();
                regs.wordregs[regax] = res16;
                break;

            case 0x27:  /* 27 DAA */
                if (((regs.byteregs[regal] & 0xF) > 9) || (af == 1)) {
                    oper1 = regs.byteregs[regal] + 6;
                    regs.byteregs[regal] = oper1 & 255;
                    if (oper1 & 0xFF00) {
                        cf = 1;
                    } else {
                        cf = 0;
                    }

                    af = 1;
                } else {
                    //af = 0;
                }

                if ((regs.byteregs[regal] > 0x9F) || (cf == 1)) {
                    regs.byteregs[regal] = regs.byteregs[regal] + 0x60;
                    cf = 1;
                } else {
                    //cf = 0;
                }

                regs.byteregs[regal] = regs.byteregs[regal] & 255;
                flag_szp8(regs.byteregs[regal]);
                break;

            case 0x28:  /* 28 SUB Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8 (reg);
                op_sub8();
                writerm8(rm, res8);
                break;

            case 0x29:  /* 29 SUB Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16 (reg);
                op_sub16();
                writerm16(rm, res16);
                break;

            case 0x2A:  /* 2A SUB Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                op_sub8();
                putreg8 (reg, res8);
                break;

            case 0x2B:  /* 2B SUB Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                op_sub16();
                putreg16 (reg, res16);
                break;

            case 0x2C:  /* 2C SUB regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                op_sub8();
                regs.byteregs[regal] = res8;
                break;

            case 0x2D:  /* 2D SUB eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                op_sub16();
                regs.wordregs[regax] = res16;
                break;

            case 0x2F:  /* 2F DAS */
                if (((regs.byteregs[regal] & 15) > 9) || (af == 1)) {
                    oper1 = regs.byteregs[regal] - 6;
                    regs.byteregs[regal] = oper1 & 255;
                    if (oper1 & 0xFF00) {
                        cf = 1;
                    } else {
                        cf = 0;
                    }

                    af = 1;
                } else {
                    af = 0;
                }

                if (((regs.byteregs[regal] & 0xF0) > 0x90) || (cf == 1)) {
                    regs.byteregs[regal] = regs.byteregs[regal] - 0x60;
                    cf = 1;
                } else {
                    cf = 0;
                }

                flag_szp8(regs.byteregs[regal]);
                break;

            case 0x30:  /* 30 XOR Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8 (reg);
                op_xor8();
                writerm8(rm, res8);
                break;

            case 0x31:  /* 31 XOR Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16 (reg);
                op_xor16();
                writerm16(rm, res16);
                break;

            case 0x32:  /* 32 XOR Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                op_xor8();
                putreg8 (reg, res8);
                break;

            case 0x33:  /* 33 XOR Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                op_xor16();
                putreg16 (reg, res16);
                break;

            case 0x34:  /* 34 XOR regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                op_xor8();
                regs.byteregs[regal] = res8;
                break;

            case 0x35:  /* 35 XOR eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                op_xor16();
                regs.wordregs[regax] = res16;
                break;

            case 0x37:  /* 37 AAA ASCII */
                if (((regs.byteregs[regal] & 0xF) > 9) || (af == 1)) {
                    regs.byteregs[regal] = regs.byteregs[regal] + 6;
                    regs.byteregs[regah] = regs.byteregs[regah] + 1;
                    af = 1;
                    cf = 1;
                } else {
                    af = 0;
                    cf = 0;
                }

                regs.byteregs[regal] = regs.byteregs[regal] & 0xF;
                break;

            case 0x38:  /* 38 CMP Eb Gb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8 (reg);
                flag_sub8(oper1b, oper2b);
                break;

            case 0x39:  /* 39 CMP Ev Gv */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16 (reg);
                flag_sub16(oper1, oper2);
                break;

            case 0x3A:  /* 3A CMP Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                flag_sub8(oper1b, oper2b);
                break;

            case 0x3B:  /* 3B CMP Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                flag_sub16(oper1, oper2);
                break;

            case 0x3C:  /* 3C CMP regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                flag_sub8(oper1b, oper2b);
                break;

            case 0x3D:  /* 3D CMP eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                flag_sub16(oper1, oper2);
                break;

            case 0x3F:  /* 3F AAS ASCII */
                if (((regs.byteregs[regal] & 0xF) > 9) || (af == 1)) {
                    regs.byteregs[regal] = regs.byteregs[regal] - 6;
                    regs.byteregs[regah] = regs.byteregs[regah] - 1;
                    af = 1;
                    cf = 1;
                } else {
                    af = 0;
                    cf = 0;
                }

                regs.byteregs[regal] = regs.byteregs[regal] & 0xF;
                break;

            case 0x40:  /* 40 INC eAX */
                oldcf = cf;
                oper1 = regs.wordregs[regax];
                oper2 = 1;
                op_add16();
                cf = oldcf;
                regs.wordregs[regax] = res16;
                break;

            case 0x41:  /* 41 INC eCX */
                oldcf = cf;
                oper1 = regs.wordregs[regcx];
                oper2 = 1;
                op_add16();
                cf = oldcf;
                regs.wordregs[regcx] = res16;
                break;

            case 0x42:  /* 42 INC eDX */
                oldcf = cf;
                oper1 = regs.wordregs[regdx];
                oper2 = 1;
                op_add16();
                cf = oldcf;
                regs.wordregs[regdx] = res16;
                break;

            case 0x43:  /* 43 INC eBX */
                oldcf = cf;
                oper1 = regs.wordregs[regbx];
                oper2 = 1;
                op_add16();
                cf = oldcf;
                regs.wordregs[regbx] = res16;
                break;

            case 0x44:  /* 44 INC eSP */
                oldcf = cf;
                oper1 = regs.wordregs[regsp];
                oper2 = 1;
                op_add16();
                cf = oldcf;
                regs.wordregs[regsp] = res16;
                break;

            case 0x45:  /* 45 INC eBP */
                oldcf = cf;
                oper1 = regs.wordregs[regbp];
                oper2 = 1;
                op_add16();
                cf = oldcf;
                regs.wordregs[regbp] = res16;
                break;

            case 0x46:  /* 46 INC eSI */
                oldcf = cf;
                oper1 = regs.wordregs[regsi];
                oper2 = 1;
                op_add16();
                cf = oldcf;
                regs.wordregs[regsi] = res16;
                break;

            case 0x47:  /* 47 INC eDI */
                oldcf = cf;
                oper1 = regs.wordregs[regdi];
                oper2 = 1;
                op_add16();
                cf = oldcf;
                regs.wordregs[regdi] = res16;
                break;

            case 0x48:  /* 48 DEC eAX */
                oldcf = cf;
                oper1 = regs.wordregs[regax];
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                regs.wordregs[regax] = res16;
                break;

            case 0x49:  /* 49 DEC eCX */
                oldcf = cf;
                oper1 = regs.wordregs[regcx];
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                regs.wordregs[regcx] = res16;
                break;

            case 0x4A:  /* 4A DEC eDX */
                oldcf = cf;
                oper1 = regs.wordregs[regdx];
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                regs.wordregs[regdx] = res16;
                break;

            case 0x4B:  /* 4B DEC eBX */
                oldcf = cf;
                oper1 = regs.wordregs[regbx];
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                regs.wordregs[regbx] = res16;
                break;

            case 0x4C:  /* 4C DEC eSP */
                oldcf = cf;
                oper1 = regs.wordregs[regsp];
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                regs.wordregs[regsp] = res16;
                break;

            case 0x4D:  /* 4D DEC eBP */
                oldcf = cf;
                oper1 = regs.wordregs[regbp];
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                regs.wordregs[regbp] = res16;
                break;

            case 0x4E:  /* 4E DEC eSI */
                oldcf = cf;
                oper1 = regs.wordregs[regsi];
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                regs.wordregs[regsi] = res16;
                break;

            case 0x4F:  /* 4F DEC eDI */
                oldcf = cf;
                oper1 = regs.wordregs[regdi];
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                regs.wordregs[regdi] = res16;
                break;

            case 0x50:  /* 50 PUSH eAX */
                push(regs.wordregs[regax]);
                break;

            case 0x51:  /* 51 PUSH eCX */
                push(regs.wordregs[regcx]);
                break;

            case 0x52:  /* 52 PUSH eDX */
                push(regs.wordregs[regdx]);
                break;

            case 0x53:  /* 53 PUSH eBX */
                push(regs.wordregs[regbx]);
                break;

            case 0x54:  /* 54 PUSH eSP */
                push(regs.wordregs[regsp] - 2);
                break;

            case 0x55:  /* 55 PUSH eBP */
                push(regs.wordregs[regbp]);
                break;

            case 0x56:  /* 56 PUSH eSI */
                push(regs.wordregs[regsi]);
                break;

            case 0x57:  /* 57 PUSH eDI */
                push(regs.wordregs[regdi]);
                break;

            case 0x58:  /* 58 POP eAX */
                regs.wordregs[regax] = pop();
                break;

            case 0x59:  /* 59 POP eCX */
                regs.wordregs[regcx] = pop();
                break;

            case 0x5A:  /* 5A POP eDX */
                regs.wordregs[regdx] = pop();
                break;

            case 0x5B:  /* 5B POP eBX */
                regs.wordregs[regbx] = pop();
                break;

            case 0x5C:  /* 5C POP eSP */
                regs.wordregs[regsp] = pop();
                break;

            case 0x5D:  /* 5D POP eBP */
                regs.wordregs[regbp] = pop();
                break;

            case 0x5E:  /* 5E POP eSI */
                regs.wordregs[regsi] = pop();
                break;

            case 0x5F:  /* 5F POP eDI */
                regs.wordregs[regdi] = pop();
                break;

#ifndef CPU_8086
            case 0x60:  /* 60 PUSHA (80186+) */
                oldsp = regs.wordregs[regsp];
                push(regs.wordregs[regax]);
                push(regs.wordregs[regcx]);
                push(regs.wordregs[regdx]);
                push(regs.wordregs[regbx]);
                push(oldsp);
                push(regs.wordregs[regbp]);
                push(regs.wordregs[regsi]);
                push(regs.wordregs[regdi]);
                break;

            case 0x61:  /* 61 POPA (80186+) */
                regs.wordregs[regdi] = pop();
                regs.wordregs[regsi] = pop();
                regs.wordregs[regbp] = pop();
                dummy = pop();
                regs.wordregs[regbx] = pop();
                regs.wordregs[regdx] = pop();
                regs.wordregs[regcx] = pop();
                regs.wordregs[regax] = pop();
                break;

            case 0x62: /* 62 BOUND Gv, Ev (80186+) */
                modregrm();
                getea(rm);
                if (signext32 (getreg16(reg)) < signext32 (getmem16(ea >> 4, ea & 15))) {
                    intcall86(5); //bounds check exception
                } else {
                    ea += 2;
                    if (signext32 (getreg16(reg)) > signext32 (getmem16(ea >> 4, ea & 15))) {
                        intcall86(5); //bounds check exception
                    }
                }
                break;

            case 0x68:  /* 68 PUSH Iv (80186+) */
                push(getmem16 (segregs[regcs], ip));
                StepIP (2);
                break;

            case 0x69:  /* 69 IMUL Gv Ev Iv (80186+) */
                modregrm();
                temp1 = readrm16(rm);
                temp2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                if ((temp1 & 0x8000L) == 0x8000L) {
                    temp1 = temp1 | 0xFFFF0000L;
                }

                if ((temp2 & 0x8000L) == 0x8000L) {
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
                break;

            case 0x6A:  /* 6A PUSH Ib (80186+) */
                push(getmem8 (segregs[regcs], ip));
                StepIP (1);
                break;

            case 0x6B:  /* 6B IMUL Gv Eb Ib (80186+) */
                modregrm();
                temp1 = readrm16(rm);
                temp2 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if ((temp1 & 0x8000L) == 0x8000L) {
                    temp1 = temp1 | 0xFFFF0000L;
                }

                if ((temp2 & 0x8000L) == 0x8000L) {
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
                break;

            case 0x6C:  /* 6E INSB */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                putmem8 (useseg, regs.wordregs[regsi], portin(regs.wordregs[regdx]));
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 1;
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 1;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 1;
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 1;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0x6D:  /* 6F INSW */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                putmem16 (useseg, regs.wordregs[regsi], portin(regs.wordregs[regdx]));
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 2;
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 2;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 2;
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 2;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0x6E:  /* 6E OUTSB */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                portout(regs.wordregs[regdx], getmem8 (useseg, regs.wordregs[regsi]));
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 1;
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 1;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 1;
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 1;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0x6F:  /* 6F OUTSW */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                portout(regs.wordregs[regdx], getmem16 (useseg, regs.wordregs[regsi]));
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 2;
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 2;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 2;
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 2;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;
#endif
            case 0x70:  /* 70 JO Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (of) {
                    ip = ip + temp16;
                }
                break;

            case 0x71:  /* 71 JNO Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (!of) {
                    ip = ip + temp16;
                }
                break;

            case 0x72:  /* 72 JB Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (cf) {
                    ip = ip + temp16;
                }
                break;

            case 0x73:  /* 73 JNB Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (!cf) {
                    ip = ip + temp16;
                }
                break;

            case 0x74:  /* 74 JZ Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x75:  /* 75 JNZ Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (!zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x76:  /* 76 JBE Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (cf || zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x77:  /* 77 JA Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (!cf && !zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x78:  /* 78 JS Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (sf) {
                    ip = ip + temp16;
                }
                break;

            case 0x79:  /* 79 JNS Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (!sf) {
                    ip = ip + temp16;
                }
                break;

            case 0x7A:  /* 7A JPE Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (pf) {
                    ip = ip + temp16;
                }
                break;

            case 0x7B:  /* 7B JPO Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (!pf) {
                    ip = ip + temp16;
                }
                break;

            case 0x7C:  /* 7C JL Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (sf != of) {
                    ip = ip + temp16;
                }
                break;

            case 0x7D:  /* 7D JGE Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (sf == of) {
                    ip = ip + temp16;
                }
                break;

            case 0x7E:  /* 7E JLE Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if ((sf != of) || zf) {
                    ip = ip + temp16;
                }
                break;

            case 0x7F:  /* 7F JG Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (!zf && (sf == of)) {
                    ip = ip + temp16;
                }
                break;

            case 0x80:
            case 0x82:  /* 80/82 GRP1 Eb Ib */
                modregrm();
                oper1b = readrm8(rm);
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
                        flag_sub8(oper1b, oper2b);
                        break;
                    default:
                        break;  /* to avoid compiler warnings */
                }

                if (reg < 7) {
                    writerm8(rm, res8);
                }
                break;

            case 0x81:  /* 81 GRP1 Ev Iv */
            case 0x83:  /* 83 GRP1 Ev Ib */
                modregrm();
                oper1 = readrm16(rm);
                if (opcode == 0x81) {
                    oper2 = getmem16 (segregs[regcs], ip);
                    StepIP (2);
                } else {
                    oper2 = signext (getmem8(segregs[regcs], ip));
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
                        flag_sub16(oper1, oper2);
                        break;
                    default:
                        break;  /* to avoid compiler warnings */
                }

                if (reg < 7) {
                    writerm16(rm, res16);
                }
                break;

            case 0x84:  /* 84 TEST Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                oper2b = readrm8(rm);
                flag_log8(oper1b & oper2b);
                break;

            case 0x85:  /* 85 TEST Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                oper2 = readrm16(rm);
                flag_log16(oper1 & oper2);
                break;

            case 0x86:  /* 86 XCHG Gb Eb */
                modregrm();
                oper1b = getreg8 (reg);
                putreg8 (reg, readrm8(rm));
                writerm8(rm, oper1b);
                break;

            case 0x87:  /* 87 XCHG Gv Ev */
                modregrm();
                oper1 = getreg16 (reg);
                putreg16 (reg, readrm16(rm));
                writerm16(rm, oper1);
                break;

            case 0x88:  /* 88 MOV Eb Gb */
                modregrm();
                writerm8(rm, getreg8 (reg));
                break;

            case 0x89:  /* 89 MOV Ev Gv */
                modregrm();
                writerm16(rm, getreg16 (reg));
                break;

            case 0x8A:  /* 8A MOV Gb Eb */
                modregrm();
                putreg8 (reg, readrm8(rm));
                break;

            case 0x8B:  /* 8B MOV Gv Ev */
                modregrm();
                putreg16 (reg, readrm16(rm));
                break;

            case 0x8C:  /* 8C MOV Ew Sw */
                modregrm();
                writerm16(rm, getsegreg (reg));
                break;

            case 0x8D:  /* 8D LEA Gv M */
                modregrm();
                getea(rm);
                putreg16 (reg, ea - segbase(useseg));
                break;

            case 0x8E:  /* 8E MOV Sw Ew */
                modregrm();
                putsegreg (reg, readrm16(rm));
                break;

            case 0x8F:  /* 8F POP Ev */
                modregrm();
                writerm16(rm, pop());
                break;

            case 0x90:  /* 90 NOP */
                break;

            case 0x91:  /* 91 XCHG eCX eAX */
                oper1 = regs.wordregs[regcx];
                regs.wordregs[regcx] = regs.wordregs[regax];
                regs.wordregs[regax] = oper1;
                break;

            case 0x92:  /* 92 XCHG eDX eAX */
                oper1 = regs.wordregs[regdx];
                regs.wordregs[regdx] = regs.wordregs[regax];
                regs.wordregs[regax] = oper1;
                break;

            case 0x93:  /* 93 XCHG eBX eAX */
                oper1 = regs.wordregs[regbx];
                regs.wordregs[regbx] = regs.wordregs[regax];
                regs.wordregs[regax] = oper1;
                break;

            case 0x94:  /* 94 XCHG eSP eAX */
                oper1 = regs.wordregs[regsp];
                regs.wordregs[regsp] = regs.wordregs[regax];
                regs.wordregs[regax] = oper1;
                break;

            case 0x95:  /* 95 XCHG eBP eAX */
                oper1 = regs.wordregs[regbp];
                regs.wordregs[regbp] = regs.wordregs[regax];
                regs.wordregs[regax] = oper1;
                break;

            case 0x96:  /* 96 XCHG eSI eAX */
                oper1 = regs.wordregs[regsi];
                regs.wordregs[regsi] = regs.wordregs[regax];
                regs.wordregs[regax] = oper1;
                break;

            case 0x97:  /* 97 XCHG eDI eAX */
                oper1 = regs.wordregs[regdi];
                regs.wordregs[regdi] = regs.wordregs[regax];
                regs.wordregs[regax] = oper1;
                break;

            case 0x98:  /* 98 CBW */
                if ((regs.byteregs[regal] & 0x80) == 0x80) {
                    regs.byteregs[regah] = 0xFF;
                } else {
                    regs.byteregs[regah] = 0;
                }
                break;

            case 0x99:  /* 99 CWD */
                if ((regs.byteregs[regah] & 0x80) == 0x80) {
                    regs.wordregs[regdx] = 0xFFFF;
                } else {
                    regs.wordregs[regdx] = 0;
                }
                break;

            case 0x9A:  /* 9A CALL Ap */
                oper1 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                push(segregs[regcs]);
                push(ip);
                ip = oper1;
                segregs[regcs] = oper2;
                break;

            case 0x9B:  /* 9B WAIT */
                break;

            case 0x9C:  /* 9C PUSHF */
                push(makeflagsword() | 0x0800);
                break;

            case 0x9D:  /* 9D POPF */
                temp16 = pop();
                decodeflagsword(temp16);
                break;

            case 0x9E:  /* 9E SAHF */
            decodeflagsword((makeflagsword() & 0xFF00) | regs.byteregs[regah]);
                break;

            case 0x9F:  /* 9F LAHF */
                regs.byteregs[regah] = makeflagsword() & 0xFF;
                break;

            case 0xA0:  /* A0 MOV regs.byteregs[regal] Ob */
                regs.byteregs[regal] = getmem8 (useseg, getmem16(segregs[regcs], ip));
                StepIP (2);
                break;

            case 0xA1:  /* A1 MOV eAX Ov */
                oper1 = getmem16 (useseg, getmem16(segregs[regcs], ip));
                StepIP (2);
                regs.wordregs[regax] = oper1;
                break;

            case 0xA2:  /* A2 MOV Ob regs.byteregs[regal] */
                putmem8 (useseg, getmem16(segregs[regcs], ip), regs.byteregs[regal]);
                StepIP (2);
                break;

            case 0xA3:  /* A3 MOV Ov eAX */
            putmem16 (useseg, getmem16(segregs[regcs], ip), regs.wordregs[regax]);
                StepIP (2);
                break;

            case 0xA4:  /* A4 MOVSB */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                putmem8 (segregs[reges], regs.wordregs[regdi], getmem8(useseg, regs.wordregs[regsi]));
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 1;
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 1;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 1;
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 1;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xA5:  /* A5 MOVSW */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                putmem16 (segregs[reges], regs.wordregs[regdi], getmem16(useseg, regs.wordregs[regsi]));
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 2;
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 2;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 2;
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 2;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xA6:  /* A6 CMPSB */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                oper1b = getmem8 (useseg, regs.wordregs[regsi]);
                oper2b = getmem8 (segregs[reges], regs.wordregs[regdi]);
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 1;
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 1;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 1;
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 1;
                }

                flag_sub8(oper1b, oper2b);
                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                if ((reptype == 1) && !zf) {
                    break;
                } else if ((reptype == 2) && (zf == 1)) {
                    break;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xA7:  /* A7 CMPSW */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                oper1 = getmem16 (useseg, regs.wordregs[regsi]);
                oper2 = getmem16 (segregs[reges], regs.wordregs[regdi]);
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 2;
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 2;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 2;
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 2;
                }

                flag_sub16(oper1, oper2);
                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
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

            case 0xA8:  /* A8 TEST regs.byteregs[regal] Ib */
                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                flag_log8(oper1b & oper2b);
                break;

            case 0xA9:  /* A9 TEST eAX Iv */
                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                flag_log16(oper1 & oper2);
                break;

            case 0xAA:  /* AA STOSB */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                putmem8 (segregs[reges], regs.wordregs[regdi], regs.byteregs[regal]);
                if (df) {
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 1;
                } else {
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 1;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAB:  /* AB STOSW */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                putmem16 (segregs[reges], regs.wordregs[regdi], regs.wordregs[regax]);
                if (df) {
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 2;
                } else {
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 2;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAC:  /* AC LODSB */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                regs.byteregs[regal] = getmem8 (useseg, regs.wordregs[regsi]);
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 1;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 1;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAD:  /* AD LODSW */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                oper1 = getmem16 (useseg, regs.wordregs[regsi]);
                regs.wordregs[regax] = oper1;
                if (df) {
                    regs.wordregs[regsi] = regs.wordregs[regsi] - 2;
                } else {
                    regs.wordregs[regsi] = regs.wordregs[regsi] + 2;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAE:  /* AE SCASB */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                oper1b = regs.byteregs[regal];
                oper2b = getmem8 (segregs[reges], regs.wordregs[regdi]);
                flag_sub8(oper1b, oper2b);
                if (df) {
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 1;
                } else {
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 1;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                if ((reptype == 1) && !zf) {
                    break;
                } else if ((reptype == 2) && (zf == 1)) {
                    break;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xAF:  /* AF SCASW */
                if (reptype && (regs.wordregs[regcx] == 0)) {
                    break;
                }

                oper1 = regs.wordregs[regax];
                oper2 = getmem16 (segregs[reges], regs.wordregs[regdi]);
                flag_sub16(oper1, oper2);
                if (df) {
                    regs.wordregs[regdi] = regs.wordregs[regdi] - 2;
                } else {
                    regs.wordregs[regdi] = regs.wordregs[regdi] + 2;
                }

                if (reptype) {
                    regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                }

                if ((reptype == 1) && !zf) {
                    break;
                } else if ((reptype == 2) & (zf == 1)) {
                    break;
                }

                totalexec++;
                loopcount++;
                if (!reptype) {
                    break;
                }

                ip = firstip;
                break;

            case 0xB0:  /* B0 MOV regs.byteregs[regal] Ib */
                regs.byteregs[regal] = getmem8 (segregs[regcs], ip);
                StepIP (1);
                break;

            case 0xB1:  /* B1 MOV regs.byteregs[regcl] Ib */
                regs.byteregs[regcl] = getmem8 (segregs[regcs], ip);
                StepIP (1);
                break;

            case 0xB2:  /* B2 MOV regs.byteregs[regdl] Ib */
                regs.byteregs[regdl] = getmem8 (segregs[regcs], ip);
                StepIP (1);
                break;

            case 0xB3:  /* B3 MOV regs.byteregs[regbl] Ib */
                regs.byteregs[regbl] = getmem8 (segregs[regcs], ip);
                StepIP (1);
                break;

            case 0xB4:  /* B4 MOV regs.byteregs[regah] Ib */
                regs.byteregs[regah] = getmem8 (segregs[regcs], ip);
                StepIP (1);
                break;

            case 0xB5:  /* B5 MOV regs.byteregs[regch] Ib */
                regs.byteregs[regch] = getmem8 (segregs[regcs], ip);
                StepIP (1);
                break;

            case 0xB6:  /* B6 MOV regs.byteregs[regdh] Ib */
                regs.byteregs[regdh] = getmem8 (segregs[regcs], ip);
                StepIP (1);
                break;

            case 0xB7:  /* B7 MOV regs.byteregs[regbh] Ib */
                regs.byteregs[regbh] = getmem8 (segregs[regcs], ip);
                StepIP (1);
                break;

            case 0xB8:  /* B8 MOV eAX Iv */
                oper1 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                regs.wordregs[regax] = oper1;
                break;

            case 0xB9:  /* B9 MOV eCX Iv */
                oper1 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                regs.wordregs[regcx] = oper1;
                break;

            case 0xBA:  /* BA MOV eDX Iv */
                oper1 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                regs.wordregs[regdx] = oper1;
                break;

            case 0xBB:  /* BB MOV eBX Iv */
                oper1 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                regs.wordregs[regbx] = oper1;
                break;

            case 0xBC:  /* BC MOV eSP Iv */
                regs.wordregs[regsp] = getmem16 (segregs[regcs], ip);
                StepIP (2);
                break;

            case 0xBD:  /* BD MOV eBP Iv */
                regs.wordregs[regbp] = getmem16 (segregs[regcs], ip);
                StepIP (2);
                break;

            case 0xBE:  /* BE MOV eSI Iv */
                regs.wordregs[regsi] = getmem16 (segregs[regcs], ip);
                StepIP (2);
                break;

            case 0xBF:  /* BF MOV eDI Iv */
                regs.wordregs[regdi] = getmem16 (segregs[regcs], ip);
                StepIP (2);
                break;

            case 0xC0:  /* C0 GRP2 byte imm8 (80186+) */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                writerm8(rm, op_grp2_8(oper2b));
                break;

            case 0xC1:  /* C1 GRP2 word imm8 (80186+) */
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getmem8 (segregs[regcs], ip);
                StepIP (1);
                writerm16(rm, op_grp2_16((uint8_t) oper2));
                break;

            case 0xC2:  /* C2 RET Iw */
                oper1 = getmem16 (segregs[regcs], ip);
                ip = pop();
                regs.wordregs[regsp] = regs.wordregs[regsp] + oper1;
                break;

            case 0xC3:  /* C3 RET */
                ip = pop();
                break;

            case 0xC4:  /* C4 LES Gv Mp */
                modregrm();
                getea(rm);
                putreg16 (reg, read86(ea) + read86(ea + 1) * 256);
                segregs[reges] = read86(ea + 2) + read86(ea + 3) * 256;
                break;

            case 0xC5:  /* C5 LDS Gv Mp */
                modregrm();
                getea(rm);
                putreg16 (reg, read86(ea) + read86(ea + 1) * 256);
                segregs[regds] = read86(ea + 2) + read86(ea + 3) * 256;
                break;

            case 0xC6:  /* C6 MOV Eb Ib */
                modregrm();
                writerm8(rm, getmem8 (segregs[regcs], ip));
                StepIP (1);
                break;

            case 0xC7:  /* C7 MOV Ev Iv */
                modregrm();
                writerm16(rm, getmem16 (segregs[regcs], ip));
                StepIP (2);
                break;

            case 0xC8:  /* C8 ENTER (80186+) */
                stacksize = getmem16 (segregs[regcs], ip);
                StepIP (2);
                nestlev = getmem8 (segregs[regcs], ip);
                StepIP (1);
                push(regs.wordregs[regbp]);
                frametemp = regs.wordregs[regsp];
                if (nestlev) {
                    for (temp16 = 1; temp16 < nestlev; temp16++) {
                        regs.wordregs[regbp] = regs.wordregs[regbp] - 2;
                        push(regs.wordregs[regbp]);
                    }

                    push(regs.wordregs[regsp]);
                }

                regs.wordregs[regbp] = frametemp;
                regs.wordregs[regsp] = regs.wordregs[regbp] - stacksize;

                break;

            case 0xC9:  /* C9 LEAVE (80186+) */
                regs.wordregs[regsp] = regs.wordregs[regbp];
                regs.wordregs[regbp] = pop();
                break;

            case 0xCA:  /* CA RETF Iw */
                oper1 = getmem16 (segregs[regcs], ip);
                ip = pop();
                segregs[regcs] = pop();
                regs.wordregs[regsp] = regs.wordregs[regsp] + oper1;
                break;

            case 0xCB:  /* CB RETF */
                ip = pop();;
                segregs[regcs] = pop();
                break;

            case 0xCC:  /* CC INT 3 */
                intcall86(3);
                break;

            case 0xCD:  /* CD INT Ib */
                oper1b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                intcall86(oper1b);
                break;

            case 0xCE:  /* CE INTO */
                if (of) {
                    intcall86(4);
                }
                break;

            case 0xCF:  /* CF IRET */
                ip = pop();
                segregs[regcs] = pop();
                decodeflagsword(pop());

                /*
             * if (net.enabled) net.canrecv = 1;
             */
                break;

            case 0xD0:  /* D0 GRP2 Eb 1 */
                modregrm();
                oper1b = readrm8(rm);
                writerm8(rm, op_grp2_8(1));
                break;

            case 0xD1:  /* D1 GRP2 Ev 1 */
                modregrm();
                oper1 = readrm16(rm);
                writerm16(rm, op_grp2_16(1));
                break;

            case 0xD2:  /* D2 GRP2 Eb regs.byteregs[regcl] */
                modregrm();
                oper1b = readrm8(rm);
                writerm8(rm, op_grp2_8(regs.byteregs[regcl]));
                break;

            case 0xD3:  /* D3 GRP2 Ev regs.byteregs[regcl] */
                modregrm();
                oper1 = readrm16(rm);
                writerm16(rm, op_grp2_16(regs.byteregs[regcl]));
                break;

            case 0xD4:  /* D4 AAM I0 */
                oper1 = getmem8 (segregs[regcs], ip);
                StepIP (1);
                if (!oper1) {
                    intcall86(0);
                    break;
                } /* division by zero */

                regs.byteregs[regah] = (regs.byteregs[regal] / oper1) & 255;
                regs.byteregs[regal] = (regs.byteregs[regal] % oper1) & 255;
                flag_szp16(regs.wordregs[regax]);
                break;

            case 0xD5:  /* D5 AAD I0 */
                oper1 = getmem8 (segregs[regcs], ip);
                StepIP (1);
                regs.byteregs[regal] = (regs.byteregs[regah] * oper1 + regs.byteregs[regal]) & 255;
                regs.byteregs[regah] = 0;
                flag_szp16(regs.byteregs[regah] * oper1 + regs.byteregs[regal]);
                sf = 0;
                break;

            case 0xD6:  /* D6 XLAT on V20/V30, SALC on 8086/8088 */
                regs.byteregs[regal] = cf ? 0xFF : 0x00;
                break;

            case 0xD7:  /* D7 XLAT */
                regs.byteregs[regal] = read86(useseg * 16 + (regs.wordregs[regbx]) + regs.byteregs[regal]);
                break;

            case 0xD8:
            case 0xD9:
            case 0xDA:
            case 0xDB:
            case 0xDC:
            case 0xDE:
            case 0xDD:
            case 0xDF:  /* escape to x87 FPU (unsupported) */
                modregrm();
                break;

            case 0xE0:  /* E0 LOOPNZ Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                if ((regs.wordregs[regcx]) && !zf) {
                    ip = ip + temp16;
                }
                break;

            case 0xE1:  /* E1 LOOPZ Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                if (regs.wordregs[regcx] && (zf == 1)) {
                    ip = ip + temp16;
                }
                break;

            case 0xE2:  /* E2 LOOP Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                regs.wordregs[regcx] = regs.wordregs[regcx] - 1;
                if (regs.wordregs[regcx]) {
                    ip = ip + temp16;
                }
                break;

            case 0xE3:  /* E3 JCXZ Jb */
                temp16 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                if (!regs.wordregs[regcx]) {
                    ip = ip + temp16;
                }
                break;

            case 0xE4:  /* E4 IN regs.byteregs[regal] Ib */
                oper1b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                regs.byteregs[regal] = (uint8_t) portin(oper1b);
                break;

            case 0xE5:  /* E5 IN eAX Ib */
                oper1b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                regs.wordregs[regax] = portin(oper1b);
                break;

            case 0xE6:  /* E6 OUT Ib regs.byteregs[regal] */
                oper1b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                portout(oper1b, regs.byteregs[regal]);
                break;

            case 0xE7:  /* E7 OUT Ib eAX */
                oper1b = getmem8 (segregs[regcs], ip);
                StepIP (1);
                portout(oper1b, regs.wordregs[regax]);
                break;

            case 0xE8:  /* E8 CALL Jv */
                oper1 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                push(ip);
                ip = ip + oper1;
                break;

            case 0xE9:  /* E9 JMP Jv */
                oper1 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                ip = ip + oper1;
                break;

            case 0xEA:  /* EA JMP Ap */
                oper1 = getmem16 (segregs[regcs], ip);
                StepIP (2);
                oper2 = getmem16 (segregs[regcs], ip);
                ip = oper1;
                segregs[regcs] = oper2;
                break;

            case 0xEB:  /* EB JMP Jb */
                oper1 = signext (getmem8(segregs[regcs], ip));
                StepIP (1);
                ip = ip + oper1;
                break;

            case 0xEC:  /* EC IN regs.byteregs[regal] regdx */
                oper1 = regs.wordregs[regdx];
                regs.byteregs[regal] = (uint8_t) portin(oper1);
                break;

            case 0xED:  /* ED IN eAX regdx */
                oper1 = regs.wordregs[regdx];
                regs.wordregs[regax] = portin(oper1);
                break;

            case 0xEE:  /* EE OUT regdx regs.byteregs[regal] */
                oper1 = regs.wordregs[regdx];
                portout(oper1, regs.byteregs[regal]);
                break;

            case 0xEF:  /* EF OUT regdx eAX */
                oper1 = regs.wordregs[regdx];
                portout(oper1, regs.wordregs[regax]);
                break;

            case 0xF0:  /* F0 LOCK */
                break;

            case 0xF4:  /* F4 HLT */
                hltstate = 1;
                break;

            case 0xF5:  /* F5 CMC */
                if (!cf) {
                    cf = 1;
                } else {
                    cf = 0;
                }
                break;

            case 0xF6:  /* F6 GRP3a Eb */
                modregrm();
                oper1b = readrm8(rm);
                op_grp3_8();
                if ((reg > 1) && (reg < 4)) {
                    writerm8(rm, res8);
                }
                break;

            case 0xF7:  /* F7 GRP3b Ev */
                modregrm();
                oper1 = readrm16(rm);
                op_grp3_16();
                if ((reg > 1) && (reg < 4)) {
                    writerm16(rm, res16);
                }
                break;

            case 0xF8:  /* F8 CLC */
                cf = 0;
                break;

            case 0xF9:  /* F9 STC */
                cf = 1;
                break;

            case 0xFA:  /* FA CLI */
                ifl = 0;
                break;

            case 0xFB:  /* FB STI */
                ifl = 1;
                break;

            case 0xFC:  /* FC CLD */
                df = 0;
                break;

            case 0xFD:  /* FD STD */
                df = 1;
                break;

            case 0xFE:  /* FE GRP4 Eb */
                modregrm();
                oper1b = readrm8(rm);
                oper2b = 1;
                if (!reg) {
                    tempcf = cf;
                    res8 = oper1b + oper2b;
                    flag_add8(oper1b, oper2b);
                    cf = tempcf;
                    writerm8(rm, res8);
                } else {
                    tempcf = cf;
                    res8 = oper1b - oper2b;
                    flag_sub8(oper1b, oper2b);
                    cf = tempcf;
                    writerm8(rm, res8);
                }
                break;

            case 0xFF:  /* FF GRP5 Ev */
                modregrm();
                oper1 = readrm16(rm);
                op_grp5();
                break;

            default:
                break;
        }

        skipexecution:
        if (!running) {
            return;
        }
    }
}

void exec861(uint32_t execloops) {
    uint32_t loopcount;
    uint8_t docontinue;
    static uint16_t firstip, trap_toggle = 0;

    for (loopcount = 0; loopcount < execloops; loopcount++) {
        if (trap_toggle) intcall86(1);
        /*if (tf) trap_toggle = 1;
        else trap_toggle = 0;*/
        trap_toggle = tf;

        if (!trap_toggle && (ifl && (i8259.irr & (~i8259.imr)))) {
            hltstate = 0;
            intcall86(nextintr());     //get next interrupt from the i8259, if any
        }
        if (hltstate) {
            break;
        }

        reptype = 0;
        segoverride = 0;
        useseg = segregs[regds];
        docontinue = 0;
        firstip = ip;
        while (!docontinue) {
            segregs[regcs] = segregs[regcs] & 0xFFFF;
            ip = ip & 0xFFFF;
            savecs = segregs[regcs];
            saveip = ip;
            opcode = getmem8(segregs[regcs], ip);
            StepIP(1);

            switch (opcode) {                                         //segment prefix check
                case 0x2E:                                              //segment segregs[regcs]
                    useseg = segregs[regcs];
                    segoverride = 1;
                    break;
                case 0x3E:                                              //segment segregs[regds]
                    useseg = segregs[regds];
                    segoverride = 1;
                    break;
                case 0x26:                                              //segment segregs[reges]
                    useseg = segregs[reges];
                    segoverride = 1;
                    break;
                case 0x36:                                              //segment segregs[regss]
                    useseg = segregs[regss];
                    segoverride = 1;
                    break;

                    //repetition prefix check
                case 0xF3: //REP/REPE/REPZ
                    reptype = 1;
                    break;
                case 0xF2: //REPNE/REPNZ
                    reptype = 2;
                    break;
                default:
                    docontinue = 1;
                    break;
            }
        }
        totalexec++;

        switch (opcode) {
            case 0x0: //00 ADD Eb Gb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_add8();
                writerm8(rm, res8);
                break;
            case 0x1: //01 ADD Ev Gv
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_add16();
                writerm16(rm, res16);
                break;
            case 0x2: //02 ADD Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_add8();
                putreg8(reg, res8);
                break;
            case 0x3: //03 ADD Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_add16();
                putreg16(reg, res16);
                break;
            case 0x4: //04 ADD regs.byteregs[regal] Ib
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                op_add8();
                regs.byteregs[regal] = res8;
                break;
            case 0x5: //05 ADD eAX Iv
                oper1 = (getreg16(regax));
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                op_add16();
                putreg16(regax, res16);
                break;
            case 0x6: //06 PUSH segregs[reges]
                push(segregs[reges]);
                break;
            case 0x7: //07 POP segregs[reges]
                segregs[reges] = pop();
                break;
            case 0x8: //08 OR Eb Gb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_or8();
                writerm8(rm, res8);
                break;
            case 0x9: //09 OR Ev Gv
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_or16();
                writerm16(rm, res16);
                break;
            case 0xA: //0A OR Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_or8();
                putreg8(reg, res8);
                break;
            case 0xB: //0B OR Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_or16();
                if ((oper1 == 0xF802) && (oper2 == 0xF802))
                    sf = 0; //cheap hack to make Wolf 3D think we're a 286 so it plays
                putreg16(reg, res16);
                break;
            case 0xC: //0C OR regs.byteregs[regal] Ib
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                op_or8();
                regs.byteregs[regal] = res8;
                break;
            case 0xD: //0D OR eAX Iv
                oper1 = getreg16(regax);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                op_or16();
                putreg16(regax, res16);
                break;
            case 0xE: //0E PUSH segregs[regcs]
                push(segregs[regcs]);
                break;
            case 0xF: //0F POP CS
                segregs[regcs] = pop();
                break;
            case 0x10: //10 ADC Eb Gb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_adc8();
                writerm8(rm, res8);
                break;
            case 0x11: //11 ADC Ev Gv
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_adc16();
                writerm16(rm, res16);
                break;
            case 0x12: //12 ADC Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_adc8();
                putreg8(reg, res8);
                break;
            case 0x13: //13 ADC Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_adc16();
                putreg16(reg, res16);
                break;
            case 0x14: //14 ADC regs.byteregs[regal] Ib
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                op_adc8();
                regs.byteregs[regal] = res8;
                break;
            case 0x15: //15 ADC eAX Iv
                oper1 = getreg16(regax);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                op_adc16();
                putreg16(regax, res16);
                break;
            case 0x16: //16 PUSH segregs[regss]
                push(segregs[regss]);
                break;
            case 0x17: //17 POP segregs[regss]
                segregs[regss] = pop();
                break;
            case 0x18: //18 SBB Eb Gb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_sbb8();
                writerm8(rm, res8);
                break;
            case 0x19: //19 SBB Ev Gv
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_sbb16();
                writerm16(rm, res16);
                break;
            case 0x1A: //1A SBB Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_sbb8();
                putreg8(reg, res8);
                break;
            case 0x1B: //1B SBB Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_sbb16();
                putreg16(reg, res16);
                break;
            case 0x1C: //1C SBB regs.byteregs[regal] Ib;
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                op_sbb8();
                regs.byteregs[regal] = res8;
                break;
            case 0x1D: //1D SBB eAX Iv
                oper1 = getreg16(regax);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                op_sbb16();
                putreg16(regax, res16);
                break;
            case 0x1E: //1E PUSH segregs[regds]
                push(segregs[regds]);
                break;
            case 0x1F: //1F POP segregs[regds]
                segregs[regds] = pop();
                break;
            case 0x20: //20 AND Eb Gb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_and8();
                writerm8(rm, res8);
                break;
            case 0x21: //21 AND Ev Gv
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_and16();
                writerm16(rm, res16);
                break;
            case 0x22: //22 AND Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_and8();
                putreg8(reg, res8);
                break;
            case 0x23: //23 AND Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_and16();
                putreg16(reg, res16);
                break;
            case 0x24: //24 AND regs.byteregs[regal] Ib
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                op_and8();
                regs.byteregs[regal] = res8;
                break;
            case 0x25: //25 AND eAX Iv
                oper1 = getreg16(regax);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                op_and16();
                putreg16(regax, res16);
                break;
            case 0x27: //27 DAA
                if (((regs.byteregs[regal] & 0xF) > 9) || (af == 1)) {
                    oper1 = regs.byteregs[regal] + 6;
                    regs.byteregs[regal] = oper1 & 255;
                    if (oper1 & 0xFF00) cf = 1; else cf = 0;
                    af = 1;
                } else af = 0;
                if (((regs.byteregs[regal] & 0xF0) > 0x90) || (cf == 1)) {
                    regs.byteregs[regal] = regs.byteregs[regal] + 0x60;
                    cf = 1;
                } else cf = 0;
                regs.byteregs[regal] = regs.byteregs[regal] & 255;
                flag_szp8(regs.byteregs[regal]);
                break;
            case 0x28: //28 SUB Eb Gb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_sub8();
                writerm8(rm, res8);
                break;
            case 0x29: //29 SUB Ev Gv
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_sub16();
                writerm16(rm, res16);
                break;
            case 0x2A: //2A SUB Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_sub8();
                putreg8(reg, res8);
                break;
            case 0x2B: //2B SUB Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_sub16();
                putreg16(reg, res16);
                break;
            case 0x2C: //2C SUB regs.byteregs[regal] Ib
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                op_sub8();
                regs.byteregs[regal] = res8;
                break;
            case 0x2D: //2D SUB eAX Iv
                oper1 = getreg16(regax);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                op_sub16();
                putreg16(regax, res16);
                break;
            case 0x2F: //2F DAS
                if (((regs.byteregs[regal] & 15) > 9) || (af == 1)) {
                    oper1 = regs.byteregs[regal] - 6;
                    regs.byteregs[regal] = oper1 & 255;
                    if (oper1 & 0xFF00) cf = 1; else cf = 0;
                    af = 1;
                } else af = 0;
                if (((regs.byteregs[regal] & 0xF0) > 0x90) || (cf == 1)) {
                    regs.byteregs[regal] = regs.byteregs[regal] - 0x60;
                    cf = 1;
                } else cf = 0;
                flag_szp8(regs.byteregs[regal]);
                break;
            case 0x30: //30 XOR Eb Gb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                op_xor8();
                writerm8(rm, res8);
                break;
            case 0x31: //31 XOR Ev Gv
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                op_xor16();
                writerm16(rm, res16);
                break;
            case 0x32: //32 XOR Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                op_xor8();
                putreg8(reg, res8);
                break;
            case 0x33: //33 XOR Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                op_xor16();
                putreg16(reg, res16);
                break;
            case 0x34: //34 XOR regs.byteregs[regal] Ib
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                op_xor8();
                regs.byteregs[regal] = res8;
                break;
            case 0x35: //35 XOR eAX Iv
                oper1 = getreg16(regax);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                op_xor16();
                putreg16(regax, res16);
                break;
            case 0x37: //37 AAA ASCII
                if (((regs.byteregs[regal] & 0xF) > 9) || (af == 1)) {
                    regs.byteregs[regal] = regs.byteregs[regal] + 6;
                    regs.byteregs[regah] = regs.byteregs[regah] + 1;
                    af = 1;
                    cf = 1;
                } else {
                    af = 0;
                    cf = 0;
                }
                regs.byteregs[regal] = regs.byteregs[regal] & 0xF;
                break;
            case 0x38: //38 CMP Eb Gb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getreg8(reg);
                flag_sub8(oper1b, oper2b);
                break;
            case 0x39: //39 CMP Ev Gv
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getreg16(reg);
                flag_sub16(oper1, oper2);
                break;
            case 0x3A: //3A CMP Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                flag_sub8(oper1b, oper2b);
                break;
            case 0x3B: //3B CMP Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                flag_sub16(oper1, oper2);
                break;
            case 0x3C: //3C CMP regs.byteregs[regal] Ib
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                flag_sub8(oper1b, oper2b);
                break;
            case 0x3D: //3D CMP eAX Iv
                oper1 = getreg16(regax);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                flag_sub16(oper1, oper2);
                break;
            case 0x3F: //3F AAS ASCII
                if (((regs.byteregs[regal] & 0xF) > 9) || (af == 1)) {
                    regs.byteregs[regal] = regs.byteregs[regal] - 6;
                    regs.byteregs[regah] = regs.byteregs[regah] - 1;
                    af = 1;
                    cf = 1;
                } else {
                    af = 0;
                    cf = 0;
                }
                regs.byteregs[regal] = regs.byteregs[regal] & 0xF;
                break;
            case 0x40: //40 INC eAX
                oldcf = cf;
                oper1 = getreg16(regax);
                oper2 = 1;
                op_add16();
                cf = oldcf;
                putreg16(regax, res16);
                break;
            case 0x41: //41 INC eCX
                oldcf = cf;
                oper1 = getreg16(regcx);
                oper2 = 1;
                op_add16();
                cf = oldcf;
                putreg16(regcx, res16);
                break;
            case 0x42: //42 INC eDX
                oldcf = cf;
                oper1 = getreg16(regdx);
                oper2 = 1;
                op_add16();
                cf = oldcf;
                putreg16(regdx, res16);
                break;
            case 0x43: //43 INC eBX
                oldcf = cf;
                oper1 = getreg16(regbx);
                oper2 = 1;
                op_add16();
                cf = oldcf;
                putreg16(regbx, res16);
                break;
            case 0x44: //44 INC eSP
                oldcf = cf;
                oper1 = getreg16(regsp);
                oper2 = 1;
                op_add16();
                cf = oldcf;
                putreg16(regsp, res16);
                break;
            case 0x45: //45 INC eBP
                oldcf = cf;
                oper1 = getreg16(regbp);
                oper2 = 1;
                op_add16();
                cf = oldcf;
                putreg16(regbp, res16);
                break;
            case 0x46: //46 INC eSI
                oldcf = cf;
                oper1 = getreg16(regsi);
                oper2 = 1;
                op_add16();
                cf = oldcf;
                putreg16(regsi, res16);
                break;
            case 0x47: //47 INC eDI
                oldcf = cf;
                oper1 = getreg16(regdi);
                oper2 = 1;
                op_add16();
                cf = oldcf;
                putreg16(regdi, res16);
                break;
            case 0x48: //48 DEC eAX
                oldcf = cf;
                oper1 = getreg16(regax);
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                putreg16(regax, res16);
                break;
            case 0x49: //49 DEC eCX
                oldcf = cf;
                oper1 = getreg16(regcx);
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                putreg16(regcx, res16);
                break;
            case 0x4A: //4A DEC eDX
                oldcf = cf;
                oper1 = getreg16(regdx);
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                putreg16(regdx, res16);
                break;
            case 0x4B: //4B DEC eBX
                oldcf = cf;
                oper1 = getreg16(regbx);
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                putreg16(regbx, res16);
                break;
            case 0x4C: //4C DEC eSP
                oldcf = cf;
                oper1 = getreg16(regsp);
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                putreg16(regsp, res16);
                break;
            case 0x4D: //4D DEC eBP
                oldcf = cf;
                oper1 = getreg16(regbp);
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                putreg16(regbp, res16);
                break;
            case 0x4E: //4E DEC eSI
                oldcf = cf;
                oper1 = getreg16(regsi);
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                putreg16(regsi, res16);
                break;
            case 0x4F: //4F DEC eDI
                oldcf = cf;
                oper1 = getreg16(regdi);
                oper2 = 1;
                op_sub16();
                cf = oldcf;
                putreg16(regdi, res16);
                break;
            case 0x50: //50 PUSH eAX
                push(getreg16(regax));
                break;
            case 0x51: //51 PUSH eCX
                push(getreg16(regcx));
                break;
            case 0x52: //52 PUSH eDX
                push(getreg16(regdx));
                break;
            case 0x53: //53 PUSH eBX
                push(getreg16(regbx));
                break;
            case 0x54: //54 PUSH eSP
                push(getreg16(regsp) - 2);
                break;
            case 0x55: //55 PUSH eBP
                push(getreg16(regbp));
                break;
            case 0x56: //56 PUSH eSI
                push(getreg16(regsi));
                break;
            case 0x57: //57 PUSH eDI
                push(getreg16(regdi));
                break;
            case 0x58: //58 POP eAX
                putreg16(regax, pop());
                break;
            case 0x59: //59 POP eCX
                putreg16(regcx, pop());
                break;
            case 0x5A: //5A POP eDX
                putreg16(regdx, pop());
                break;
            case 0x5B: //5B POP eBX
                putreg16(regbx, pop());
                break;
            case 0x5C: //5C POP eSP
                putreg16(regsp, pop());
                break;
            case 0x5D: //5D POP eBP
                putreg16(regbp, pop());
                break;
            case 0x5E: //5E POP eSI
                putreg16(regsi, pop());
                break;
            case 0x5F: //5F POP eDI
                putreg16(regdi, pop());
                break;
            case 0x60: //60 PUSHA (80186+)
                oldsp = getreg16(regsp);
                push(getreg16(regax));
                push(getreg16(regcx));
                push(getreg16(regdx));
                push(getreg16(regbx));
                push(oldsp);
                push(getreg16(regbp));
                push(getreg16(regsi));
                push(getreg16(regdi));
                break;
            case 0x61: //61 POPA (80186+)
                putreg16(regdi, pop());
                putreg16(regsi, pop());
                putreg16(regbp, pop());
                dummy = pop();
                putreg16(regbx, pop());
                putreg16(regdx, pop());
                putreg16(regcx, pop());
                putreg16(regax, pop());
                break;
            case 0x68: //68 PUSH Iv (80186+)
                push(getmem16(segregs[regcs], ip));
                StepIP(2);
                break;
            case 0x69: //69 IMUL Gv Ev Iv (80186+)
                //print("WE HIT 69h IMUL\r\n");
                modregrm();
                temp1 = readrm16(rm);
                temp2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                if ((temp1 & 0x8000L) == 0x8000L) temp1 = temp1 | 0xFFFF0000L;
                if ((temp2 & 0x8000L) == 0x8000L) temp2 = temp2 | 0xFFFF0000L;
                temp3 = temp1 * temp2;
                putreg16(reg, temp3 & 0xFFFFL);
                if (temp3 & 0xFFFF0000L) {
                    cf = 1;
                    of = 1;
                } else {
                    cf = 0;
                    of = 0;
                }
                break;
            case 0x6A: //6A PUSH Ib (80186+)
                push(getmem8(segregs[regcs], ip));
                StepIP(1);
                break;
            case 0x6B: //6B IMUL Gv Eb Ib (80186+)
                modregrm();
                temp1 = readrm16(rm);
                temp2 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if ((temp1 & 0x8000L) == 0x8000L) temp1 = temp1 | 0xFFFF0000L;
                if ((temp2 & 0x8000L) == 0x8000L) temp2 = temp2 | 0xFFFF0000L;
                temp3 = temp1 * temp2;
                putreg16(reg, temp3 & 0xFFFFL);
                if (temp3 & 0xFFFF0000L) {
                    cf = 1;
                    of = 1;
                } else {
                    cf = 0;
                    of = 0;
                }
                break;
            case 0x6C:
                if (reptype && (getreg16(regcx) == 0)) break;
                putmem8(useseg, getreg16(regsi), portin(getreg16(regdx)));
                if (df) {
                    putreg16(regsi, getreg16(regsi) - 1);
                    putreg16(regdi, getreg16(regdi) - 1);
                } else {
                    putreg16(regsi, getreg16(regsi) + 1);
                    putreg16(regdi, getreg16(regdi) + 1);
                }

                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;
/* FIXME!!
      case 0x6D:
            if (reptype && (getreg16(regcx) == 0)) break;
            putmem16(useseg, getreg16(regsi), uint16_t (portin(getreg16(regdx)) & (portin(getreg16(regdx)+1))<<8));
            if (df) {
                putreg16(regsi, getreg16(regsi) - 2);
                putreg16(regdi, getreg16(regdi) - 2);
            } else {
                putreg16(regsi, getreg16(regsi) + 2);
                putreg16(regdi, getreg16(regdi) + 2);
            }
            if (reptype) putreg16(regcx, getreg16(regcx) - 1);
            if (!reptype) break;
            ip = firstip;
            break;
            */
                // WS added 0x6C and 0x6D (17-Nov-21) based on the Fake86+ by lgblgblgb. Not sure if it works or not??????

                //case 0x6C ... 0x6F: //80186 port operations, just act as if they//re NOPs for now...
                //    StepIP(1); //they have a modregrm(); byte we must skip... i may properly emulate these later.
                //    break;
            case 0x6E: //6E OUTSB
                if (reptype && (getreg16(regcx) == 0)) break;
                portout16 = 0;
                portout(regs.wordregs[regdx], getmem16(useseg, getreg16(regsi)));
                if (df) {
                    putreg16(regsi, getreg16(regsi) - 1);
                    putreg16(regdi, getreg16(regdi) - 1);
                } else {
                    putreg16(regsi, getreg16(regsi) + 1);
                    putreg16(regdi, getreg16(regdi) + 1);
                }
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;
            case 0x6F: //6F OUTSW
                if (reptype && (getreg16(regcx) == 0)) break;
                portout16 = 1;
                portout(regs.wordregs[regdx], getmem16(useseg, getreg16(regsi)));
                if (df) {
                    putreg16(regsi, getreg16(regsi) - 2);
                    putreg16(regdi, getreg16(regdi) - 2);
                } else {
                    putreg16(regsi, getreg16(regsi) + 2);
                    putreg16(regdi, getreg16(regdi) + 2);
                }
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;

            case 0x70: //70 JO Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (of) ip = ip + temp16;
                break;
            case 0x71: //71 JNO Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (!of) ip = ip + temp16;
                break;
            case 0x72: //72 JB Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (cf) ip = ip + temp16;
                break;
            case 0x73: //73 JNB Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (!cf) ip = ip + temp16;
                break;
            case 0x74: //74 JZ Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (zf) ip = ip + temp16;
                break;
            case 0x75: //75 JNZ Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (!zf) ip = ip + temp16;
                break;
            case 0x76: //76 JBE Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (cf || zf) ip = ip + temp16;
                break;
            case 0x77: //77 JA Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (!cf && !zf) ip = ip + temp16;
                break;
            case 0x78: //78 JS Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (sf) ip = ip + temp16;
                break;
            case 0x79: //79 JNS Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (!sf) ip = ip + temp16;
                break;
            case 0x7A: //7A JPE Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (pf) ip = ip + temp16;
                break;
            case 0x7B: //7B JPO Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (!pf) ip = ip + temp16;
                break;
            case 0x7C: //7C JL Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (sf != of) ip = ip + temp16;
                break;
            case 0x7D: //7D JGE Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (sf == of) ip = ip + temp16;
                break;
            case 0x7E: //7E JLE Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if ((sf != of) || zf) ip = ip + temp16;
                break;
            case 0x7F: //7F JG Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (!zf && (sf == of)) ip = ip + temp16;
                break;
            case 0x80:
            case 0x82: //80/82 GRP1 Eb Ib
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getmem8(segregs[regcs], ip);
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
                        break; //to avoid compiler warnings
                }
                if (reg < 7) writerm8(rm, res8);
                break;
            case 0x81: //81 GRP1 Ev Iv
            case 0x83: //83 GRP1 Ev Ib
                modregrm();
                oper1 = readrm16(rm);
                if (opcode == 0x81) {
                    oper2 = getmem16(segregs[regcs], ip);
                    StepIP(2);
                } else {
                    oper2 = signext(getmem8(segregs[regcs], ip));
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
                        break; //to avoid compiler warnings
                }
                if (reg < 7) writerm16(rm, res16);
                break;
            case 0x84: //84 TEST Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                oper2b = readrm8(rm);
                flag_log8(oper1b & oper2b);
                break;
            case 0x85: //85 TEST Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                oper2 = readrm16(rm);
                flag_log16(oper1 & oper2);
                break;
            case 0x86: //86 XCHG Gb Eb
                modregrm();
                oper1b = getreg8(reg);
                putreg8(reg, readrm8(rm));
                writerm8(rm, oper1b);
                break;
            case 0x87: //87 XCHG Gv Ev
                modregrm();
                oper1 = getreg16(reg);
                putreg16(reg, readrm16(rm));
                writerm16(rm, oper1);
                break;
            case 0x88: //88 MOV Eb Gb
                modregrm();
                writerm8(rm, getreg8(reg));
                break;
            case 0x89: //89 MOV Ev Gv
                modregrm();
                writerm16(rm, getreg16(reg));
                break;
            case 0x8A: //8A MOV Gb Eb
                modregrm();
                putreg8(reg, readrm8(rm));
                break;
            case 0x8B: //8B MOV Gv Ev
                modregrm();
                putreg16(reg, readrm16(rm));
                break;
            case 0x8C: //8C MOV Ew Sw
                modregrm();
                writerm16(rm, getsegreg(reg));
                break;
            case 0x8D: //8D LEA Gv M
                modregrm();
                //getea(rm);
                putreg16(reg, ea - segbase(useseg));
                break;
            case 0x8E: //8E MOV Sw Ew
                modregrm();
                putsegreg(reg, readrm16(rm));
                break;
            case 0x8F: //8F POP Ev
                modregrm();
                writerm16(rm, pop());
                break;
            case 0x90: //90 NOP
                break;
            case 0x91: //91 XCHG eCX eAX
                oper1 = getreg16(regcx);
                putreg16(regcx, getreg16(regax));
                putreg16(regax, oper1);
                break;
            case 0x92: //92 XCHG eDX eAX
                oper1 = getreg16(regdx);
                putreg16(regdx, getreg16(regax));
                putreg16(regax, oper1);
                break;
            case 0x93: //93 XCHG eBX eAX
                oper1 = getreg16(regbx);
                putreg16(regbx, getreg16(regax));
                putreg16(regax, oper1);
                break;
            case 0x94: //94 XCHG eSP eAX
                oper1 = getreg16(regsp);
                putreg16(regsp, getreg16(regax));
                putreg16(regax, oper1);
                break;
            case 0x95: //95 XCHG eBP eAX
                oper1 = getreg16(regbp);
                putreg16(regbp, getreg16(regax));
                putreg16(regax, oper1);
                break;
            case 0x96: //96 XCHG eSI eAX
                oper1 = getreg16(regsi);
                putreg16(regsi, getreg16(regax));
                putreg16(regax, oper1);
                break;
            case 0x97: //97 XCHG eDI eAX
                oper1 = getreg16(regdi);
                putreg16(regdi, getreg16(regax));
                putreg16(regax, oper1);
                break;
            case 0x98: //98 CBW
                if ((regs.byteregs[regal] & 0x80) == 0x80) regs.byteregs[regah] = 0xFF;
                else regs.byteregs[regah] = 0;
                break;
            case 0x99: //99 CWD
                if ((regs.byteregs[regah] & 0x80) == 0x80) putreg16(regdx, 0xFFFF);
                else
                    putreg16(regdx, 0);
                break;
            case 0x9A: //9A CALL Ap
                oper1 = getmem16(segregs[regcs], ip);
                StepIP(2);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                push(segregs[regcs]);
                push(ip);
                ip = oper1;
                segregs[regcs] = oper2;
                break;
            case 0x9B: //9B WAIT
                break;
            case 0x9C: //9C PUSHF
                push(makeflagsword() | 0xF800);
                break;
            case 0x9D: //9D POPF
                temp16 = pop();
                decodeflagsword(temp16);
                break;
            case 0x9E: //9E SAHF
            decodeflagsword ((makeflagsword() & 0xFF00) | regs.byteregs[regah]);
                break;
            case 0x9F: //9F LAHF
                regs.byteregs[regah] = makeflagsword() & 0xFF;
                break;
            case 0xA0: //A0 MOV regs.byteregs[regal] Ob
                regs.byteregs[regal] = getmem8(useseg, getmem16(segregs[regcs], ip));
                StepIP(2);
                break;
            case 0xA1: //A1 MOV eAX Ov
                oper1 = getmem16(useseg, getmem16(segregs[regcs], ip));
                StepIP(2);
                putreg16(regax, oper1);
                break;
            case 0xA2: //A2 MOV Ob regs.byteregs[regal]
                putmem8(useseg, getmem16(segregs[regcs], ip), regs.byteregs[regal]);
                StepIP(2);
                break;
            case 0xA3: //A3 MOV Ov eAX
            putmem16(useseg, getmem16(segregs[regcs], ip), getreg16(regax));
                StepIP(2);
                break;
            case 0xA4: //A4 MOVSB
                if (reptype && (getreg16(regcx) == 0)) break;
                putmem8(segregs[reges], getreg16(regdi), getmem8(useseg, getreg16(regsi)));
                if (df) {
                    putreg16(regsi, getreg16(regsi) - 1);
                    putreg16(regdi, getreg16(regdi) - 1);
                } else {
                    putreg16(regsi, getreg16(regsi) + 1);
                    putreg16(regdi, getreg16(regdi) + 1);
                }
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xA5: //A5 MOVSW
                if (reptype && (getreg16(regcx) == 0)) break;
                putmem16(segregs[reges], getreg16(regdi), getmem16(useseg, getreg16(regsi)));
                if (df) {
                    putreg16(regsi, getreg16(regsi) - 2);
                    putreg16(regdi, getreg16(regdi) - 2);
                } else {
                    putreg16(regsi, getreg16(regsi) + 2);
                    putreg16(regdi, getreg16(regdi) + 2);
                }
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xA6: //A6 CMPSB
                if (reptype && (getreg16(regcx) == 0)) break;
                oper1b = getmem8(useseg, getreg16(regsi));
                oper2b = getmem8(segregs[reges], getreg16(regdi));
                if (df) {
                    putreg16(regsi, getreg16(regsi) - 1);
                    putreg16(regdi, getreg16(regdi) - 1);
                } else {
                    putreg16(regsi, getreg16(regsi) + 1);
                    putreg16(regdi, getreg16(regdi) + 1);
                }
                flag_sub8(oper1b, oper2b);
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if ((reptype == 1) && !zf) break;
                else if ((reptype == 2) && (zf == 1)) break;
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xA7: //A7 CMPSW
                if (reptype && (getreg16(regcx) == 0)) break;
                oper1 = getmem16(useseg, getreg16(regsi));
                oper2 = getmem16(segregs[reges], getreg16(regdi));
                if (df) {
                    putreg16(regsi, getreg16(regsi) - 2);
                    putreg16(regdi, getreg16(regdi) - 2);
                } else {
                    putreg16(regsi, getreg16(regsi) + 2);
                    putreg16(regdi, getreg16(regdi) + 2);
                }
                flag_sub16(oper1, oper2);
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if ((reptype == 1) && !zf) break;
                if ((reptype == 2) && (zf == 1)) break;
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xA8: //A8 TEST regs.byteregs[regal] Ib
                oper1b = regs.byteregs[regal];
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                flag_log8(oper1b & oper2b);
                break;
            case 0xA9: //A9 TEST eAX Iv
                oper1 = getreg16(regax);
                oper2 = getmem16(segregs[regcs], ip);
                StepIP(2);
                flag_log16(oper1 & oper2);
                break;
            case 0xAA: //AA STOSB
                if (reptype && (getreg16(regcx) == 0)) break;
                putmem8(segregs[reges], getreg16(regdi), regs.byteregs[regal]);
                if (df) putreg16(regdi, getreg16(regdi) - 1);
                else
                    putreg16(regdi, getreg16(regdi) + 1);
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xAB: //AB STOSW
                if (reptype && (getreg16(regcx) == 0)) break;
                putmem16(segregs[reges], getreg16(regdi), getreg16(regax));
                if (df) putreg16(regdi, getreg16(regdi) - 2);
                else
                    putreg16(regdi, getreg16(regdi) + 2);
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xAC: //AC LODSB
                if (reptype && (getreg16(regcx) == 0)) break;
                regs.byteregs[regal] = getmem8(useseg, getreg16(regsi));
                if (df) putreg16(regsi, getreg16(regsi) - 1);
                else
                    putreg16(regsi, getreg16(regsi) + 1);
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xAD: //AD LODSW
                if (reptype && (getreg16(regcx) == 0)) break;
                oper1 = getmem16(useseg, getreg16(regsi));
                putreg16(regax, oper1);
                if (df) putreg16(regsi, getreg16(regsi) - 2);
                else
                    putreg16(regsi, getreg16(regsi) + 2);
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xAE: //AE SCASB
                if (reptype && (getreg16(regcx) == 0)) break;
                oper1b = getmem8(segregs[reges], getreg16(regdi));
                oper2b = regs.byteregs[regal];
                flag_sub8(oper1b, oper2b);
                if (df) putreg16(regdi, getreg16(regdi) - 1);
                else
                    putreg16(regdi, getreg16(regdi) + 1);
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if ((reptype == 1) && !zf) break;
                else if ((reptype == 2) && (zf == 1)) break;
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xAF: //AF SCASW
                if (reptype && (getreg16(regcx) == 0)) break;
                oper1 = getmem16(segregs[reges], getreg16(regdi));
                oper2 = getreg16(regax);
                flag_sub16(oper1, oper2);
                if (df) putreg16(regdi, getreg16(regdi) - 2);
                else
                    putreg16(regdi, getreg16(regdi) + 2);
                if (reptype) putreg16(regcx, getreg16(regcx) - 1);
                if ((reptype == 1) && !zf) break;
                else if ((reptype == 2) & (zf == 1)) break;
                if (!reptype) break;
                ip = firstip;
                break;
            case 0xB0: //B0 MOV regs.byteregs[regal] Ib
                regs.byteregs[regal] = getmem8(segregs[regcs], ip);
                StepIP(1);
                break;
            case 0xB1: //B1 MOV regs.byteregs[regcl] Ib
                regs.byteregs[regcl] = getmem8(segregs[regcs], ip);
                StepIP(1);
                break;
            case 0xB2: //B2 MOV regs.byteregs[regdl] Ib
                regs.byteregs[regdl] = getmem8(segregs[regcs], ip);
                StepIP(1);
                break;
            case 0xB3: //B3 MOV regs.byteregs[regbl] Ib
                regs.byteregs[regbl] = getmem8(segregs[regcs], ip);
                StepIP(1);
                break;
            case 0xB4: //B4 MOV regs.byteregs[regah] Ib
                regs.byteregs[regah] = getmem8(segregs[regcs], ip);
                StepIP(1);
                break;
            case 0xB5: //B5 MOV regs.byteregs[regch] Ib
                regs.byteregs[regch] = getmem8(segregs[regcs], ip);
                StepIP(1);
                break;
            case 0xB6: //B6 MOV regs.byteregs[regdh] Ib
                regs.byteregs[regdh] = getmem8(segregs[regcs], ip);
                StepIP(1);
                break;
            case 0xB7: //B7 MOV regs.byteregs[regbh] Ib
                regs.byteregs[regbh] = getmem8(segregs[regcs], ip);
                StepIP(1);
                break;
            case 0xB8: //B8 MOV eAX Iv
                oper1 = getmem16(segregs[regcs], ip);
                StepIP(2);
                putreg16(regax, oper1);
                break;
            case 0xB9: //B9 MOV eCX Iv
                oper1 = getmem16(segregs[regcs], ip);
                StepIP(2);
                putreg16(regcx, oper1);
                break;
            case 0xBA: //BA MOV eDX Iv
                oper1 = getmem16(segregs[regcs], ip);
                StepIP(2);
                putreg16(regdx, oper1);
                break;
            case 0xBB: //BB MOV eBX Iv
                oper1 = getmem16(segregs[regcs], ip);
                StepIP(2);
                putreg16(regbx, oper1);
                break;
            case 0xBC: //BC MOV eSP Iv
                putreg16(regsp, getmem16(segregs[regcs], ip));
                StepIP(2);
                break;
            case 0xBD: //BD MOV eBP Iv
                putreg16(regbp, getmem16(segregs[regcs], ip));
                StepIP(2);
                break;
            case 0xBE: //BE MOV eSI Iv
                putreg16(regsi, getmem16(segregs[regcs], ip));
                StepIP(2);
                break;
            case 0xBF: //BF MOV eDI Iv
                putreg16(regdi, getmem16(segregs[regcs], ip));
                StepIP(2);
                break;
            case 0xC0: //C0 GRP2 byte imm8 (80186+)
                modregrm();
                oper1b = readrm8(rm);
                oper2b = getmem8(segregs[regcs], ip);
                StepIP(1);
                writerm8(rm, op_grp2_8(oper2b));
                break;
            case 0xC1: //C1 GRP2 word imm8 (80186+)
                modregrm();
                oper1 = readrm16(rm);
                oper2 = getmem8(segregs[regcs], ip);
                StepIP(1);
                writerm16(rm, op_grp2_16(oper2));
                break;
            case 0xC2: //C2 RET Iw
                oper1 = getmem16(segregs[regcs], ip);
                ip = pop();
                putreg16(regsp, getreg16(regsp) + oper1);
                break;
            case 0xC3: //C3 RET
                ip = pop();
                break;
            case 0xC4: //C4 LES Gv Mp
                modregrm();
                //getea(rm);
                putreg16(reg, read86(ea) + ((uint16_t) read86(ea + 1) << 8));
                segregs[reges] = read86(ea + 2) + ((uint16_t) read86(ea + 3) << 8);
                break;
            case 0xC5: //C5 LDS Gv Mp
                modregrm();
                //getea(rm);
                putreg16(reg, read86(ea) + ((uint16_t) read86(ea + 1) << 8));
                segregs[regds] = read86(ea + 2) + ((uint16_t) read86(ea + 3) << 8);
                break;
            case 0xC6: //C6 MOV Eb Ib
                modregrm();
                writerm8(rm, getmem8(segregs[regcs], ip));
                StepIP(1);
                break;
            case 0xC7: //C7 MOV Ev Iv
                modregrm();
                writerm16(rm, getmem16(segregs[regcs], ip));
                StepIP(2);
                break;
            case 0xC8: //C8 ENTER (80186+)
                stacksize = getmem16(segregs[regcs], ip);
                StepIP(2);
                nestlev = getmem8(segregs[regcs], ip);
                StepIP(1);
                push(getreg16(regbp));
                frametemp = getreg16(regsp);
                if (nestlev) {
                    for (temp16 = 1; temp16 < nestlev; temp16++) {
                        putreg16(regbp, getreg16(regbp) - 2);
                        push(getreg16(regbp));
                    }
                    push(getreg16(regsp));
                }
                putreg16(regbp, frametemp);
                putreg16(regsp, getreg16(regbp) - stacksize);

                break;
            case 0xC9: //C9 LEAVE (80186+)
                putreg16(regsp, getreg16(regbp));
                putreg16(regbp, pop());

                break;
            case 0xCA: //CA RETF Iw
                oper1 = getmem16(segregs[regcs], ip);
                ip = pop();
                segregs[regcs] = pop();
                putreg16(regsp, getreg16(regsp) + oper1);
                break;
            case 0xCB: //CB RETF
                ip = pop();;
                segregs[regcs] = pop();
                break;
            case 0xCC: //CC INT 3
                intcall86(3);
                break;
            case 0xCD: //CD INT Ib
                oper1 = getmem8(segregs[regcs], ip);
                StepIP(1);
                intcall86(oper1);
                break;
            case 0xCE: //CE INTO
                if (of) intcall86(4);
                break;
            case 0xCF: //CF IRET
                ip = pop();
                segregs[regcs] = pop();
                decodeflagsword(pop());
                break;
            case 0xD0: //D0 GRP2 Eb 1
                modregrm();
                oper1b = readrm8(rm);
                writerm8(rm, op_grp2_8(1));
                break;
            case 0xD1: //D1 GRP2 Ev 1
                modregrm();
                oper1 = readrm16(rm);
                writerm16(rm, op_grp2_16(1));
                break;
            case 0xD2: //D2 GRP2 Eb regs.byteregs[regcl]
                modregrm();
                oper1b = readrm8(rm);
                writerm8(rm, op_grp2_8(regs.byteregs[regcl]));
                break;
            case 0xD3: //D3 GRP2 Ev regs.byteregs[regcl]
                modregrm();
                oper1 = readrm16(rm);
                writerm16(rm, op_grp2_16(regs.byteregs[regcl]));
                break;
            case 0xD4: //D4 AAM I0
                oper1 = getmem8(segregs[regcs], ip);
                StepIP(1);
                if (!oper1) {
                    intcall86(0);  //division by zero
                    return;
                }
                regs.byteregs[regah] = (regs.byteregs[regal] / oper1) & 255;
                regs.byteregs[regal] = (regs.byteregs[regal] % oper1) & 255;
                flag_szp16(getreg16(regax));
                break;
            case 0xD5: //D5 AAD I0
                oper1 = getmem8(segregs[regcs], ip);
                StepIP(1);
                regs.byteregs[regal] = (regs.byteregs[regah] * oper1 + regs.byteregs[regal]) & 255;
                regs.byteregs[regah] = 0;
                flag_szp16(regs.byteregs[regah] * oper1 + regs.byteregs[regal]);
                sf = 0;
                break;
            case 0xD6: //D6 XLAT on V20/V30, SALC on 8086/8088
                regs.byteregs[regal] = cf;
                break;
            case 0xD7: //D7 XLAT
                putreg8(regal, read86(segbase(useseg) + (uint32_t) getreg16(regbx) + (uint32_t) getreg8(regal)));
                break;
            case 0xD8:
            case 0xD9:
            case 0xDA:
            case 0xDB:
            case 0xDC:
            case 0xDE:
            case 0xDD:
            case 0xDF: //escape
                StepIP(1);
                break;
            case 0xE0: //E0 LOOPNZ Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                putreg16(regcx, getreg16(regcx) - 1);
                if ((getreg16(regcx)) && !zf) ip = ip + temp16;
                break;
            case 0xE1: //E1 LOOPZ Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                putreg16(regcx, (getreg16(regcx)) - 1);
                if ((getreg16(regcx)) && (zf == 1)) ip = ip + temp16;
                break;
            case 0xE2: //E2 LOOP Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                putreg16(regcx, (getreg16(regcx)) - 1);
                if (getreg16(regcx)) ip = ip + temp16;
                break;
            case 0xE3: //E3 JCXZ Jb
                temp16 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                if (!(getreg16(regcx))) ip = ip + temp16;
                break;
            case 0xE4: //E4 IN regs.byteregs[regal] Ib
                oper1b = getmem8(segregs[regcs], ip);
                StepIP(1);
                regs.byteregs[regal] = portin(oper1b);
                break;
            case 0xE5: //E5 IN eAX Ib
                oper1b = getmem8(segregs[regcs], ip);
                StepIP(1);
                putreg16(regax, portin(oper1b));
                break;
            case 0xE6: //E6 OUT Ib regs.byteregs[regal]
                oper1b = getmem8(segregs[regcs], ip);
                StepIP(1);
                portout16 = 0;
                portout(oper1b, regs.byteregs[regal]);
                break;
            case 0xE7: //E7 OUT Ib eAX
                oper1b = getmem8(segregs[regcs], ip);
                StepIP(1);
                portout16 = 1;
                portout(oper1b, (getreg16(regax)));
                break;
            case 0xE8: //E8 CALL Jv
                oper1 = getmem16(segregs[regcs], ip);
                StepIP(2);
                push(ip);
                ip = ip + oper1;
                break;
            case 0xE9: //E9 JMP Jv
                oper1 = getmem16(segregs[regcs], ip);
                StepIP(2);
                ip = ip + oper1;
                break;
            case 0xEA: //EA JMP Ap
                oper1 = getmem16(segregs[regcs], ip);
                StepIP(2);
                oper2 = getmem16(segregs[regcs], ip);
                ip = oper1;
                segregs[regcs] = oper2;
                break;
            case 0xEB: //EB JMP Jb
                oper1 = signext(getmem8(segregs[regcs], ip));
                StepIP(1);
                ip = ip + oper1;
                break;
            case 0xEC: //EC IN regs.byteregs[regal] regdx
                oper1 = (getreg16(regdx));
                regs.byteregs[regal] = portin(oper1);
                break;
            case 0xED: //ED IN eAX regdx
                oper1 = (getreg16(regdx));
                putreg16(regax, portin(oper1));
                break;
            case 0xEE: //EE OUT regdx regs.byteregs[regal]
                oper1 = (getreg16(regdx));
                portout16 = 0;
                portout(oper1, regs.byteregs[regal]);
                break;
            case 0xEF: //EF OUT regdx eAX
                oper1 = (getreg16(regdx));
                portout16 = 1;
                portout(oper1, (getreg16(regax)));
                break;
            case 0xF0: //F0 LOCK
            case 0xF4: //F4 HLT
                hltstate = 1;
                break;
            case 0xF5: //F5 CMC
                if (!cf) cf = 1;
                else cf = 0;
                break;
            case 0xF6: //F6 GRP3a Eb
                modregrm();
                oper1b = readrm8(rm);
                op_grp3_8();
                if ((reg > 1) && (reg < 4)) writerm8(rm, res8);
                break;
            case 0xF7: //F7 GRP3b Ev
                modregrm();
                oper1 = readrm16(rm);
                op_grp3_16();
                if ((reg > 1) && (reg < 4)) writerm16(rm, res16);
                break;
            case 0xF8: //F8 CLC
                cf = 0;
                break;
            case 0xF9: //F9 STC
                cf = 1;
                break;
            case 0xFA: //FA CLI
                ifl = 0;
                break;
            case 0xFB: //FB STI
                ifl = 1;
                break;
            case 0xFC: //FC CLD
                df = 0;
                break;
            case 0xFD: //FD STD
                df = 1;
                break;
            case 0xFE: //FE GRP4 Eb
                modregrm();
                oper1b = readrm8(rm);
                oper2b = 1;
                if (!reg) {
                    tempcf = cf;
                    res8 = oper1b + oper2b;
                    flag_add8(oper1b, oper2b);
                    cf = tempcf;
                    writerm8(rm, res8);
                } else {
                    tempcf = cf;
                    res8 = oper1b - oper2b;
                    flag_sub8(oper1b, oper2b);
                    cf = tempcf;
                    writerm8(rm, res8);
                }
                break;
            case 0xFF: //FF GRP5 Ev
                modregrm();
                oper1 = readrm16(rm);
                op_grp5();
                break;
            default:
                intcall86(6);          //trip invalid opcode exception (this occurs on the 80186+, 8086/8088 CPUs treat them as NOPs
                break;
        }
    }
}


//=================================================================================== I8259 ====================================================================================

struct structpic i8259;

void init8259() {
    memset((void *) &i8259, 0, sizeof(i8259));
}

uint8_t in8259(uint16_t portnum) {
    switch (portnum & 1) {
        case 0:
            if (i8259.readmode == 0) return (i8259.irr);
            else return (i8259.isr);
        case 1: //read mask register
            return (i8259.imr);
    }
    return (0); //can't get here, but the compiler bitches
}

extern uint32_t makeupticks;

void out8259(uint16_t portnum, uint8_t value) {
    uint8_t i;
    switch (portnum & 1) {
        case 0:
            if (value & 0x10) { //begin initialization sequence
                i8259.icwstep = 1;
                i8259.imr = 0; //clear interrupt mask register
                i8259.icw[i8259.icwstep++] = value;
                return;
            }
            if ((value & 0x98) == 8) { //it's an OCW3
                if (value & 2) i8259.readmode = value & 2;
            }
            if (value & 0x20) { //EOI command
                for (i = 0; i < 8; i++)
                    if ((i8259.isr >> i) & 1) {
                        i8259.isr ^= (1 << i);
                        if ((i == 0) && (makeupticks > 0)) {
                            makeupticks = 0;
                            i8259.irr |= 1;
                        }
                        return;
                    }
            }
            break;
        case 1:
            if ((i8259.icwstep == 3) && (i8259.icw[1] & 2)) i8259.icwstep = 4; //single mode, so don't read ICW3
            if (i8259.icwstep < 5) {
                i8259.icw[i8259.icwstep++] = value;
                return;
            }
            //if we get to this point, this is just a new IMR value
            i8259.imr = value;
            break;
    }
}

uint8_t nextintr() {
    uint8_t i, tmpirr;
    tmpirr = i8259.irr & (~i8259.imr); //XOR request register with inverted mask register
    for (i = 0; i < 8; i++)
        if ((tmpirr >> i) & 1) {
            i8259.irr ^= (1 << i);
            i8259.isr |= (1 << i);
            return (i8259.icw[2] + i);
        }
    return (0); //can't get here, but the compiler bitches
}

void doirq(uint8_t irqnum) {
    i8259.irr |= (1 << irqnum);
}

//============================================================================= PORT ==========================================================================================


void portout(uint16_t portnum, uint16_t value) {
    if (portnum < 256) portram[portnum] = value;

    switch (portnum) {
        case 0x20:
        case 0x21: //i8259
            out8259(portnum, value);
            return;
        case 0x40: //pit 0 data port
            switch (pit0command) {
                case 0xB6:
                case 0x34:
                case 0x36:
                    if (pit0latch == 0) {
                        pit0divisor = (pit0divisor & 0xFF00) + (value & 0xFF);
                        pit0latch = 1;
                        return;
                    } else {
                        pit0divisor = (pit0divisor & 0xFF) + (value & 0xFF) * 256;
                        pit0latch = 0;
                        if (pit0divisor == 0) pit0divisor = 65536;
                        return;
                    }
            }
            break;
        case 0x42: //speaker countdown
            if (latch42 == 0) {
                speakercountdown = (speakercountdown & 0xFF00) + value;
                latch42 = 1;
            } else {
                speakercountdown = (speakercountdown & 0xFF) + value * 256;
                latch42 = 0;
                speakercountdown = speakercountdown / 10;

                if (speakercountdown == 0) {
                    /* FIXME!!
                    analogWrite(SPK_PIN, 0);                      // set 0% (0) duty clcle ==> Sound output off
                    digitalWrite(SPK_PIN, LOW);                   // set the pin LOW to avoid current flowing through speaker.
                     */
                } else {
                    /* FIXME!!
                      SPK_FREQ = (238500/speakercountdown);             // Calculate the resulting frequency of the counter output

                  //  SPK_FREQ = (477000/speakercountdown);             // Calculate the resulting frequency of the counter output --- wrong frequency?? 2x!

                    if (SPK_FREQ < 20000) {                           // Human audible frequency
                        analogWriteFreq(SPK_FREQ);                    // set PWM frequency.
                        SPK_FREQ = 1;
                    } else {
                        analogWrite(SPK_PIN, 0);                      // set 0% (0) duty clcle ==> Sound output off
                        digitalWrite(SPK_PIN, LOW);                   // set the pin LOW to avoid current flowing through speaker.
                        SPK_FREQ = 1;
                    }
                    if (portram[0x61]&2){
                        analogWrite(SPK_PIN, 127);                    // set 50% (127) duty cycle ==> Sound output on
                    } else {
                        analogWrite(SPK_PIN, 0);                      // set 0% (0) duty clcle ==> Sound output off
                        digitalWrite(SPK_PIN, LOW);                   // set the pin LOW to avoid current flowing through speaker.
                    }
                                   */
                }
            }
            break;

        case 0x43: //pit 0 command port
            pit0command = value;
            switch (pit0command) {
                case 0x34:
                case 0x36: //reprogram pit 0 divisor
                    pit0latch = 0;
                    break;
                default:
                    latch42 = 0;
                    break;
            }
            break;

        case 0x61:
            portram[0x61] = value;
            /* FIXME!!
            if ((value&2) && (speakercountdown >0)) {
                  analogWrite(SPK_PIN, 127);                    // set 50% (127) duty cycle ==> Sound output on
            } else {
                  analogWrite(SPK_PIN, 0);                      // set 0% (0) duty clcle ==> Sound output off
                  digitalWrite(SPK_PIN, LOW);                   // set the pin LOW to avoid current flowing through speaker.
            }
             */
            break;

        case 0x64:
            break;

        case 0x3D4:
            crt_controller_idx = value;
            break;

        case 0x3D5:
            break;

        case 0x3D9:
            pr3D9 = value;
            break;

        case 0x3F8:               ////////////////////// WS: comm 1
            pr3F8 = value;
            if (pr3FB & 0x80) {
                divisorLatchWord |= value;                    // Divisor Latch Low Byte
                /* FIXME!!
                switch (divisorLatchWord) {
                    case 0x900: COM1baud = 50;  break;
                    case 0x417: COM1baud = 110; break;
                    case 0x20C: COM1baud = 220; break;
                    case 0x180: COM1baud = 300; break;
                    case 0xC0:  COM1baud = 600; break;
                    case 0x60:  COM1baud = 1200; break;
                    case 0x30:  COM1baud = 2400; break;
                    case 0x18:  COM1baud = 4800; break;
                    case 0x0C:  COM1baud = 9600; break;
                    case 0x06:  COM1baud = 19200; break;
                    case 0x03:  COM1baud = 38400; break;
                    case 0x02:  COM1baud = 57600; break;
                    case 0x01:  COM1baud = 115200; break;
                    default:    COM1baud = 2400; break;
                }
                swSer1.end();
                swSer1.begin(COM1baud, COM1Config, SWSER_RX, SWSER_TX, false, SWSERBUFCAP, SWSERISRCAP);
                */
            } else {
                COM1OUT = value;
                pr3FA = 1;
            }
            break;

        case 0x3F9:
            if (pr3FB & 0x80) {
                divisorLatchWord = (value << 8);               // Divisor Latch High Byte
            } else {
                pr3F9 = value;
            }
            break;

        case 0x3FA:
            pr3FA = value;
            break;

        case 0x3FB:
            pr3FB = value;
            /* FIXME!!
            switch (pr3FB & 0x3F) {
                case 0x00:  COM1Config = SWSERIAL_5N1; break;
                case 0x01:  COM1Config = SWSERIAL_6N1; break;
                case 0x02:  COM1Config = SWSERIAL_7N1; break;
                case 0x03:  COM1Config = SWSERIAL_8N1; break;
                case 0x04:  COM1Config = SWSERIAL_5N2; break;
                case 0x05:  COM1Config = SWSERIAL_6N2; break;
                case 0x06:  COM1Config = SWSERIAL_7N2; break;
                case 0x07:  COM1Config = SWSERIAL_8N2; break;
                case 0x08:  COM1Config = SWSERIAL_5O1; break;
                case 0x09:  COM1Config = SWSERIAL_6O1; break;
                case 0x0A:  COM1Config = SWSERIAL_7O1; break;
                case 0x0B:  COM1Config = SWSERIAL_8O1; break;
                case 0x0C:  COM1Config = SWSERIAL_5O2; break;
                case 0x0D:  COM1Config = SWSERIAL_6O2; break;
                case 0x0E:  COM1Config = SWSERIAL_7O2; break;
                case 0x0F:  COM1Config = SWSERIAL_8O2; break;
                case 0x18:  COM1Config = SWSERIAL_5E1; break;
                case 0x19:  COM1Config = SWSERIAL_6E1; break;
                case 0x1A:  COM1Config = SWSERIAL_7E1; break;
                case 0x1B:  COM1Config = SWSERIAL_8E1; break;
                case 0x1C:  COM1Config = SWSERIAL_5E2; break;
                case 0x1D:  COM1Config = SWSERIAL_6E2; break;
                case 0x1E:  COM1Config = SWSERIAL_7E2; break;
                case 0x1F:  COM1Config = SWSERIAL_8E2; break;
                case 0x28:  COM1Config = SWSERIAL_5M1; break;
                case 0x29:  COM1Config = SWSERIAL_6M1; break;
                case 0x2A:  COM1Config = SWSERIAL_7M1; break;
                case 0x2B:  COM1Config = SWSERIAL_8M1; break;
                case 0x2C:  COM1Config = SWSERIAL_5M2; break;
                case 0x2D:  COM1Config = SWSERIAL_6M2; break;
                case 0x2E:  COM1Config = SWSERIAL_7M2; break;
                case 0x2F:  COM1Config = SWSERIAL_8M2; break;
                case 0x38:  COM1Config = SWSERIAL_5S1; break;
                case 0x39:  COM1Config = SWSERIAL_6S1; break;
                case 0x3A:  COM1Config = SWSERIAL_7S1; break;
                case 0x3B:  COM1Config = SWSERIAL_8S1; break;
                case 0x3C:  COM1Config = SWSERIAL_5S2; break;
                case 0x3D:  COM1Config = SWSERIAL_6S2; break;
                case 0x3E:  COM1Config = SWSERIAL_7S2; break;
                case 0x3F:  COM1Config = SWSERIAL_8S2; break;
                default: COM1Config = SWSERIAL_8N1; break;
              }
            swSer1.end();
            swSer1.begin(COM1baud, COM1Config, SWSER_RX, SWSER_TX, false, SWSERBUFCAP, SWSERISRCAP);
            */
            break;

        case 0x3FC:
            pr3FC = value;
            break;

        case 0x3FD:
            pr3FD = value;
            break;

        case 0x3FE:
            pr3FE = value;
            break;

        case 0x3FF:
            pr3FF = value;
            break;
    }
}

uint16_t portin(uint16_t portnum) {

    switch (portnum) {
        case 0x20:
        case 0x21: //i8259
            return (in8259(portnum));
        case 0x40:
            if (pit0latch == 0) {
                pit0counter = (millis() % 55) * 1192;
                pit0latch = 1;
                return (pit0counter & 0xFF);
            } else {
                pit0latch = 0;
                return ((pit0counter >> 8) & 0xFF);
            }
        case 0x43:
            return (pit0command);

        case 0x60:
            return portram[portnum];

        case 0x61:
            return portram[portnum];
            break;

        case 0x64:
            return portram[portnum];

        case 0x3D4:
            return crt_controller_idx;
            break;

        case 0x3D8:
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
            break;

        case 0x3D9:
            return pr3D9;
            break;

        case 0x3F8:
            if (COM1IN) {
                pr3F8 = COM1IN;
                COM1IN = 0;
                pr3FA = 1;
                return (pr3F8);
            } else {
                return (pr3F8);
            }
            break;

        case 0x3F9:
            return (pr3F9);

        case 0x3FA:
            if (pr3FArst == 1) {
                pr3FA = 1;
                pr3FArst = 0;
            }

            if (pr3FA != 1) pr3FArst = 1;
            return (pr3FA);

        case 0x3FB:
            return (pr3FB);

        case 0x3FC:
            return (pr3FC);

        case 0x3FD:
            return (pr3FD);

        case 0x3FE:
            return (0xBB);

        case 0x3FF:
            return (pr3FF);

        case 0x3D5:
            return crt_controller[crt_controller_idx];
            break;

        case 0x3DA:
            port3da = random();
            return (port3da);
        default:
            return (0xFF);
    }
}

//============================================================================== TIMING ==================================================================================

uint32_t timer;
extern uint32_t pit0divisor;
extern uint16_t pit0counter;

Uint32 ClockTick(Uint32 interval, void *name) {
    doirq(0);
}

//=============================================================================== MAIN ===================================================================================
#include "DOS.h"

void setup() {
    CopyCharROM();                                                 // move charROM into screenmem for faster access


// BOOT_HDD
// insertdisk(0x80);
// bootdrive = 0x80;

// BOOT_FDD
    insertdisk(0x00, sizeof FD0, FD0);
    bootdrive = 0x00;

// BOOT_BASIC
// bootdrive = 0xFF;

    RAM[0x417] = 00;
    reset86();
    tickNumber = 0;
    tickRemainder = 0;
    SDL_AddTimer(55, ClockTick, "timer");
    //clockirq.attach_ms(55, ClockTick);
}


bool runEvery(unsigned long interval) {                                   // cursor blinking timing
    static unsigned long previousMillis = 0;
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        return true;
    }
    return false;
}

bool tickerIncrease(unsigned long interval) {                             // accurate adjustment for the 18.2/s ticker
    static unsigned long sinceMillis = 0;
    currentMillis = millis();

    if (currentMillis - sinceMillis >= interval) {
        tempdummy = (currentMillis - sinceMillis) * 182;
        tempdummy = tempdummy + tickRemainder;
        dummy = tempdummy / 10000;
        tickRemainder = tempdummy % 10000;
        tickNumber = tickNumber + dummy;
        if (tickNumber > 1573039) {      /// every 1573040 = 1 day
            tickNumber = tickNumber - 1573040;
            SRAM_write(0x470, 1);
        }
        SRAM_write(0x46c, ((tickNumber >> 0) & 0xFF));
        SRAM_write(0x46d, ((tickNumber >> 8) & 0xFF));
        SRAM_write(0x46e, ((tickNumber >> 16) & 0xFF));
        SRAM_write(0x46f, ((tickNumber >> 24) & 0xFF));
        sinceMillis = currentMillis;
        return true;
    }
    return false;
}
#include <conio.h>

void loop() {
    exec86(20000);

    /* FIXME!! */
    if (false) {
        c = 32;
        if (c != 0x80) {
            previous_c = c;
            if ((c == 0x34) && (RAM[0x417] & 0x0c)) {
                curshow = false;
                reset86();
            } else {
                doirq(1);
            }
        } else {
            portram[0x60] = previous_c + 0x80;
            doirq(1);
        }
    }
    /* */

    if (tickerIncrease(1000)) {}                                            // add 18.2 ticks for every second passed...

    if (runEvery(CURSOR_SPEED)) {                                           // blink the cursor
        if (curshow) {
            if (curset) curset = false;
            else if (!curset) curset = true;
        } else {
            curset = false;
        }
    }

    /* FIXME!!
    if (swSer1.available()) {               // COM1 input
        COM1IN = swSer1.read();
        pr3F8 = COM1IN;                     // place data into holding register
        pr3FA = 4;                          // received data available register interrupt
        doirq(4);                           // trigger IRQ4
        exec86(100);                        // run 100 instructions to process the incoming data
    }

    if (COM1OUT) {                          // COM1 output
        char chr= COM1OUT;
        swSer1.print(chr);                  // send data to Serial port
        pr3FD |= 0x60;                      // transmitter holding register empty
        COM1OUT=0;                          // clear data buffer
        pr3FA = 2;                          // transmitter holding register interrupt
        pr3FArst = 0;
        doirq(4);                           // trigger IRQ4
    }
     */
}
