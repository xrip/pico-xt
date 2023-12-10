//
// Created by xrip on 20.10.2023.
//
#pragma once
#ifndef TINY8086_CPU8086_H
#define TINY8086_CPU8086_H
#define INLINE static

// Settings for max 4MB 0f PSRAM
#define TOTAL_VIRTUAL_MEMORY_KBS (4ul << 10)

#ifdef XMS_DRIVER
#if XMS_OVER_HMA_KB
#define ON_BOARD_RAM_KB (1024ul + XMS_OVER_HMA_KB)
#else
#define ON_BOARD_RAM_KB (4ul << 10)
#endif
#else
#define ON_BOARD_RAM_KB 1024ul
#endif

#define BASE_X86_KB 1024ul
#define TOTAL_XMM_KB (ON_BOARD_RAM_KB - BASE_X86_KB)

#ifdef EMS_DRIVER
//#define TOTAL_EMM_KB (TOTAL_VIRTUAL_MEMORY_KBS - ON_BOARD_RAM_KB)
#define TOTAL_EMM_KB (2ul << 10)
#else
#define TOTAL_EMM_KB 0
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <memory.h>
#include "cga.h"
#include "a20.h"
#include "emm.h"
#include "ram_page.h"
#include "../assets/rom.h"
#include "../assets/startup_disk.h"
#include "../assets/fdd.h"
//#define CPU_8086
#if PICO_ON_DEVICE
#include <hardware/pwm.h>
#include <hardware/gpio.h>
#include <pico/time.h>
#include <pico/stdlib.h>

#include "f_util.h"
#include "ff.h"
static FATFS fs;
#endif

#define BEEPER_PIN 28

#define VGA_plane_size 16000
// TODO: no direct access support (for PC mode)
#ifndef PSRAM_ONLY_NO_RAM
extern uint8_t RAM[RAM_SIZE];
#endif
extern uint8_t VIDEORAM[VIDEORAM_SIZE];
extern bool PSRAM_AVAILABLE;
extern bool SD_CARD_AVAILABLE;
extern uint32_t DIRECT_RAM_BORDER;

void init_cpu_addresses_map();

#define regax 0
#define regcx 1
#define regdx 2
#define regbx 3
#define regsp 4
#define regbp 5
#define regsi 6
#define regdi 7
#define reges 0
#define regcs 1
#define regss 2
#define regds 3

#define regal 0
#define regah 1
#define regcl 2
#define regch 3
#define regdl 4
#define regdh 5
#define regbl 6
#define regbh 7
extern uint8_t opcode, segoverride, reptype, bootdrive, hdcount, fdcount, hltstate;
extern uint16_t segregs[4], savecs, saveip, ip, useseg, oldsp;
extern uint8_t tempcf, oldcf, cf, pf, af, zf, sf, tf, ifl, df, of, mode, reg, rm;
extern uint8_t videomode;
extern uint8_t speakerenabled;
extern int timer_period;
extern uint32_t ega_plane_offset;
extern bool vga_planar_mode;

#if PICO_ON_DEVICE
extern pwm_config config;
#endif

static inline uint16_t makeflagsword(void) {
    return 2 | (uint16_t)cf | ((uint16_t)pf << 2) | ((uint16_t)af << 4) | ((uint16_t)zf << 6) |
           ((uint16_t)sf << 7) |
           ((uint16_t)tf << 8) | ((uint16_t)ifl << 9) | ((uint16_t)df << 10) | ((uint16_t)of << 11);
}

static inline void decodeflagsword(uint16_t x) {
    cf = x & 1;
    pf = (x >> 2) & 1;
    af = (x >> 4) & 1;
    zf = (x >> 6) & 1;
    sf = (x >> 7) & 1;
    tf = (x >> 8) & 1;
    ifl = (x >> 9) & 1;
    df = (x >> 10) & 1;
    of = (x >> 11) & 1;
}

#define CPU_FL_CF    cf
#define CPU_FL_PF    pf
#define CPU_FL_AF    af
#define CPU_FL_ZF    zf
#define CPU_FL_SF    sf
#define CPU_FL_TF    tf
#define CPU_FL_IFL    ifl
#define CPU_FL_DF    df
#define CPU_FL_OF    of

#define CPU_CS        segregs[regcs]
#define CPU_DS        segregs[regds]
#define CPU_ES        segregs[reges]
#define CPU_SS        segregs[regss]

#define CPU_AX    regs.wordregs[regax]
#define CPU_BX    regs.wordregs[regbx]
#define CPU_CX    regs.wordregs[regcx]
#define CPU_DX    regs.wordregs[regdx]
#define CPU_SI    regs.wordregs[regsi]
#define CPU_DI    regs.wordregs[regdi]
#define CPU_BP    regs.wordregs[regbp]
#define CPU_SP    regs.wordregs[regsp]
#define CPU_IP        ip

#define CPU_AL    regs.byteregs[regal]
#define CPU_BL    regs.byteregs[regbl]
#define CPU_CL    regs.byteregs[regcl]
#define CPU_DL    regs.byteregs[regdl]
#define CPU_AH    regs.byteregs[regah]
#define CPU_BH    regs.byteregs[regbh]
#define CPU_CH    regs.byteregs[regch]
#define CPU_DH    regs.byteregs[regdh]

// TODO: remove this trash
void logMsg(char*);

#define StepIP(x)  ip += x

#define getmem8(x, y) read86(segbase(x) + y)
#define getmem16(x, y)  readw86(segbase(x) + y)
#define putmem8(x, y, z)  write86(segbase(x) + y, z)
#define putmem16(x, y, z) writew86(segbase(x) + y, z)
#define signext(value)  (int16_t)(int8_t)(value)
#define signext32(value)  (int32_t)(int16_t)(value)
#define getreg16(regid) regs.wordregs[regid]
#define getreg8(regid)  regs.byteregs[byteregtable[regid]]
#define putreg16(regid, writeval) regs.wordregs[regid] = writeval
#define putreg8(regid, writeval)  regs.byteregs[byteregtable[regid]] = writeval
#define getsegreg(regid)  segregs[regid]
#define putsegreg(regid, writeval)  segregs[regid] = writeval
#define segbase(x)  ((uint32_t) x << 4)

extern uint16_t portram[256];
extern uint16_t port378, port379, port37A, port3D8, port3D9, port201;

extern union _bytewordregs_ {
    uint16_t wordregs[8];
    uint8_t byteregs[8];
} regs;

void diskhandler();

uint8_t insertdisk(uint8_t drivenum, size_t size, char* ROM, char* pathname);

void writew86(uint32_t addr32, uint16_t value);

void write86(uint32_t addr32, uint8_t value);

uint16_t readw86(uint32_t addr32);

uint8_t read86(uint32_t addr32);

static __inline void pokeb(uint32_t a, uint32_t b) {
    uint8_t av = read86(a);
    uint8_t bv = read86(b);
    write86(b, av);
    write86(a, bv);
}

#define peekb(a)   read86(a)

static __inline void pokew(int a, uint16_t w) {
    pokeb(a, w & 0xFF);
    pokeb(a + 1, w >> 8);
}

static __inline uint16_t peekw(int a) {
    return peekb(a) + (peekb(a + 1) << 8);
}

void reset86(void);

void exec86(uint32_t execloops);

void portout(uint16_t portnum, uint16_t value);

uint16_t portin(uint16_t portnum);

void portout16(uint16_t portnum, uint16_t value);

uint16_t portin16(uint16_t portnum);

void init8259();

void out8259(uint16_t portnum, uint8_t value);

uint8_t in8259(uint16_t portnum);

uint8_t nextintr();

void doirq(uint8_t irqnum);

void init8253();

void out8253(uint16_t portnum, uint8_t value);

uint8_t in8253(uint16_t portnum);

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
#ifdef DMA_8237
void i8237_writeport(uint16_t addr, uint8_t value);
void i8237_writepage(uint16_t addr, uint8_t value);
uint8_t i8237_readport(uint16_t addr);
uint8_t i8237_readpage(uint16_t addr);
uint8_t i8237_read(uint8_t ch);
void i8237_write(uint8_t ch, uint8_t value);
void i8237_reset();
uint8_t i8237_read(uint8_t ch);
#endif
uint8_t insermouse(uint16_t portnum);

void outsermouse(uint16_t portnum, uint8_t value);

void sermouseevent(uint8_t buttons, int8_t xrel, int8_t yrel);

void initsermouse(uint16_t baseport, uint8_t irq);

void outsoundsource(uint16_t portnum, uint8_t value);

uint8_t insoundsource(uint16_t portnum);

int16_t tickssource();

extern int16_t adlibgensample(void);

extern uint8_t inadlib(uint16_t portnum);

extern void initadlib(uint16_t baseport);

extern void outadlib(uint16_t portnum, uint8_t value);

extern void tickadlib(void);


extern int16_t getBlasterSample(void);

extern void tickBlaster(void);

extern void initBlaster(uint16_t baseport, uint8_t irq);

extern void outBlaster(uint16_t portnum, uint8_t value);

extern uint8_t inBlaster(uint16_t portnum);

#if !PICO_ON_DEVICE
void handleinput(void);
#define logMsg(c) printf("%s\r\n",c);

#endif

extern struct i8259_s {
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

extern struct i8253_s {
    uint16_t chandata[3];
    uint8_t accessmode[3];
    uint8_t bytetoggle[3];
    uint32_t effectivedata[3];
    float chanfreq[3];
    uint8_t active[3];
    uint16_t counter[3];
} i8253;

#define rgb(r, g, b) ((r<<16) | (g << 8 ) | b )
#define rgb1(b, g, r) r | (g<<8) | (b<<16);
//r<<16) | (g << 8 ) | b )
#endif //TINY8086_CPU8086_H

void notify_a20_line_state_changed(bool v);

bool is_a20_line_open();

void ports_reboot();


