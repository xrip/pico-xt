//
// Created by xrip on 20.10.2023.
//
#pragma once

#ifndef TINY8086_CPU8086_H
#define TINY8086_CPU8086_H

#include <stdint.h>
#include <stdio.h>
//#define DEBUG
#define CPU_8086
#define STACK_LENGTH 16

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

static inline uint16_t makeflagsword(void) {
    return 2 | (uint16_t) cf | ((uint16_t) pf << 2) | ((uint16_t) af << 4) | ((uint16_t) zf << 6) |
           ((uint16_t) sf << 7) |
           ((uint16_t) tf << 8) | ((uint16_t) ifl << 9) | ((uint16_t) df << 10) | ((uint16_t) of << 11);
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

#define VRAM_SIZE 32
#define RAM_SIZE (200)
extern uint8_t VRAM[VRAM_SIZE << 10];
extern uint8_t RAM[RAM_SIZE << 10];



#define pokeb(a, b) RAM[a]=(b)
#define peekb(a)   RAM[a]

static inline void pokew(int a, uint16_t w) {
    pokeb(a, w & 0xFF);
    pokeb(a + 1, w >> 8);
}

static inline uint16_t peekw(int a) {
    return peekb(a) + (peekb(a + 1) << 8);
}

extern union _bytewordregs_ {
    uint16_t wordregs[8];
    uint8_t byteregs[8];
} regs;


extern void write86(uint32_t addr32, uint8_t value);

extern void reset86(void);

extern void exec86(uint32_t execloops);

extern uint8_t read86(uint32_t addr32);

extern uint16_t readw86(uint32_t addr32);

extern uint16_t read_keyboard(void);


extern int cpu_hlt_handler(void);

void handleinput(void);

#endif //TINY8086_CPU8086_H
