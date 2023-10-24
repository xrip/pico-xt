#pragma once

#ifndef TINY8086_CPU8086_H
#define TINY8086_CPU8086_H

// BEGIN ARDUINO86 USER CONFIGURABLE OPTIONS

//SRAM_SIZE should be your total size MINUS 16384 bytes, or if
//using networking 19456 bytes! The top of your SRAM needs to
//be used for CGA video and optionally ethernet packet memory.
//if you have more than 656 KB of RAM accessible, enter 655360
//or if more than 659 KB when using networking.

#include "stdint.h"
#include "stdbool.h"


#define SRAM_SIZE 655360UL

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

extern uint8_t tempcf, oldcf, cf, pf, af, zf, sf, tf, ifl, df, of, mode, reg, rm;
extern uint16_t segregs[4], savecs, saveip, ip, useseg, oldsp;

#define StepIP(x) ip+=x
#define getmem8(x, y) read86(segbase(x)+(uint32_t)y)
//#define getmem16(x,y) (read86(segbase(x)+y) | ((uint16_t)read86(segbase(x)+y+1)<<8))
#define getmem16(x, y) readw86(segbase(x)+(uint32_t)y)
#define putmem8(x, y, z) write86(segbase(x)+(uint32_t)y, z)
//#define putmem16(x,y,z) write86(segbase(x)+y, ((z)&0xFF)); write86(segbase(x)+y+1, (((z)>>8)&0xFF))
#define putmem16(x, y, z) writew86(segbase(x)+(uint32_t)y, z)
#define signext(value) ((((uint16_t)value&0x80)*0x1FE)|(uint16_t)value)
#define signext32(value) ((((uint32_t)value&0x8000)*0x1FFFE)|(uint32_t)value)
#define getreg16(regid) regs.wordregs[regid]
#define getreg8(regid) regs.byteregs[byteregtable[regid]]
#define putreg16(regid, writeval) regs.wordregs[regid] = writeval
#define putreg8(regid, writeval) regs.byteregs[byteregtable[regid]] = writeval
#define getsegreg(regid) segregs[regid]
#define putsegreg(regid, writeval) segregs[regid] = writeval
#define segbase(x) ((uint32_t)x<<4)


extern union _bytewordregs_ {
    uint16_t wordregs[8];
    uint8_t byteregs[8];
} regs;

#define makeflagsword() (2 | (uint16_t)cf | ((uint16_t)pf << 2) | ((uint16_t)af << 4) | ((uint16_t)zf << 6) \
        | ((uint16_t)sf << 7) | ((uint16_t)tf << 8) | ((uint16_t)ifl << 9) | ((uint16_t)df << 10) | ((uint16_t)of << 11))

#define decodeflagsword(x) {\
        temp16 = x;\
        cf = temp16 & 1;\
        pf = (temp16 >> 2) & 1;\
        af = (temp16 >> 4) & 1;\
        zf = (temp16 >> 6) & 1;\
        sf = (temp16 >> 7) & 1;\
        tf = (temp16 >> 8) & 1;\
        ifl = (temp16 >> 9) & 1;\
        df = (temp16 >> 10) & 1;\
        of = (temp16 >> 11) & 1;\
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

extern uint8_t RAM[1124 << 10];
extern uint8_t screenmem[16384];

extern void exec86(uint32_t execloops);

extern void write86(uint32_t addr32, uint8_t value);

extern uint8_t read86(uint32_t addr32);

extern void setup();

extern void loop();

#endif