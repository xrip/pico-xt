//Uncomment MEGA define if using a Mega 2560, otherwise leave undefined if using a Teensy 3.6
//#define MEGA
#include "stdint.h"
#include "stdbool.h"
#include "stdio.h"
#include "memory.h"

#ifdef MEGA
  #define RAM_SIZE 507904UL
  #define NATIVE_RAM 0UL //1024UL
  #define NATIVE_START 0UL
  //2048UL

  #define SD_PIN 10

  #define PS2_DATA 20
  #define PS2_IRQ 21

  #define NET_PIN 40

  #define ROM_READ(a,b) pgm_read_byte(a + b)

  #define SPI_CLOCK_SDCARD SPI_CLOCK_DIV64
  #define SPI_CLOCK_SPIRAM SPI_CLOCK_DIV2
#else
  #define RAM_SIZE 655360
  #define NATIVE_RAM (200 << 10)
  #define NATIVE_START 0UL

  #define SD_PIN BUILTIN_SDCARD

  #define PS2_DATA 18
  #define PS2_IRQ 19

  #define NET_PIN 29
  //#define USE_ENC28J60

  #define ROM_READ(a,b) a[b]

  #define SPI_CLOCK_SDCARD SPI_CLOCK_DIV2
  #define SPI_CLOCK_SPIRAM SPI_CLOCK_DIV2
  #define SPI_CLOCK_ENC28J60 SPI_CLOCK_DIV32
  #define SPI_CLOCK_LCD SPI_CLOCK_DIV2
#endif

#define INCLUDE_ROM_BASIC
#define PS2_KEYBOARD
//#define USE_TFT

//#define BOOT_FDD
#define BOOT_HDD
//#define BOOT_BASIC

//#define FDD_180K
//#define FDD_320K
//#define FDD_360K
//#define FDD_720K
//#define FDD_122M
//#define FDD_144M

//#define ADVANCED_CLIENT
//#define USB_DISK

#define BAUD_RATE 1000000

//#define USE_NETWORKING
//#define USE_PARALLEL

//#define PROFILING

// END ARDUINO86 USER CONFIGURABLE OPTIONS

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

extern uint16_t segregs[6], savecs, saveip, ip, useseg, oldsp;
extern uint8_t tempcf, oldcf, cf, pf, af, zf, sf, tf, ifl, df, of, mode, reg, rm;
extern uint8_t vidmode, hdcount, fdcount, bootdrive;
extern uint8_t port3DA, port3D9;
extern uint8_t portram[0x400];

extern union _bytewordregs_ {
    uint16_t wordregs[8];
    uint8_t byteregs[8];
} regs;


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


#define StepIP(x) ip+=x
#define getmem8(x,y) read86(segbase(x)+(uint32_t)y)
//#define getmem16(x,y) (read86(segbase(x)+y) | ((uint16_t)read86(segbase(x)+y+1)<<8))
#define getmem16(x,y) readw86(segbase(x)+(uint32_t)y)
#define putmem8(x,y,z) write86(segbase(x)+(uint32_t)y, z)
//#define putmem16(x,y,z) write86(segbase(x)+y, ((z)&0xFF)); write86(segbase(x)+y+1, (((z)>>8)&0xFF))
#define putmem16(x,y,z) writew86(segbase(x)+(uint32_t)y, z)
#define signext(value) ((((uint16_t)value&0x80)*0x1FE)|(uint16_t)value)
#define signext32(value) ((((uint32_t)value&0x8000)*0x1FFFE)|(uint32_t)value)
#define getreg16(regid) regs.wordregs[regid]
#define getreg8(regid) regs.byteregs[byteregtable[regid]]
#define putreg16(regid, writeval) regs.wordregs[regid] = writeval
#define putreg8(regid, writeval) regs.byteregs[byteregtable[regid]] = writeval
#define getsegreg(regid) segregs[regid]
#define putsegreg(regid, writeval) segregs[regid] = writeval
#define segbase(x) ((uint32_t)x<<4)

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

#ifdef MEGA
  #define RAM_write(a,v) {\
    uint32_t address;\
    uint8_t pinidx;\
    pinidx = (a)>>17;\
    address = (a)&0x1FFFF;\
    digitalWrite(SPI_RAM_pins[pinidx], LOW);\
    SPI.transfer(2);\
    SPI.transfer((char)(address >> 16));\
    SPI.transfer((char)(address >> 8));\
    SPI.transfer((char)address);\
    SPI.transfer(v);\
    digitalWrite(SPI_RAM_pins[pinidx], HIGH);\
  }

  #define RAM_read(a,v) {\
    uint32_t address;\
    uint8_t pinidx;\
    pinidx = (a)>>17;\
    address = (a)&0x1FFFF;\
    digitalWrite(SPI_RAM_pins[pinidx], LOW);\
    SPI.transfer(3);\
    SPI.transfer((char)(address >> 16));\
    SPI.transfer((char)(address >> 8));\
    SPI.transfer((char)address);\
    v = SPI.transfer(0xFF);\
    digitalWrite(SPI_RAM_pins[pinidx], HIGH);\
  }
#else //Teensy 3.6
#define RAM_write(a,v) printf("%s 0x%x %x\r\n", "write RAM", a, v);
  #define RAM_write1(a,v) {\
    uint32_t address;\
    uint8_t pinidx;\
    pinidx = (a)>>18;\
    address = (a)&0x3FFFF;\
    digitalWriteFast(SPI_RAM_pins[pinidx], LOW);\
    SPI1.transfer(2);\
    SPI1.transfer((char)(address >> 16));\
    SPI1.transfer((char)(address >> 8));\
    SPI1.transfer((char)address);\
    SPI1.transfer(v);\
    digitalWriteFast(SPI_RAM_pins[pinidx], HIGH);\
  }
#define RAM_read(a,v) { printf("%s 0x%x\r\n", "read RAM", a); v = 0; }
  #define RAM_read1(a,v) {\
    uint32_t address;\
    uint8_t pinidx;\
    pinidx = (a)>>18;\
    address = (a)&0x3FFFF;\
    digitalWriteFast(SPI_RAM_pins[pinidx], LOW);\
    SPI1.transfer(3);\
    SPI1.transfer((char)(address >> 16));\
    SPI1.transfer((char)(address >> 8));\
    SPI1.transfer((char)address);\
    v = SPI1.transfer(0xFF);\
    digitalWriteFast(SPI_RAM_pins[pinidx], HIGH);\
  }
#endif
extern uint8_t VRAM[];
void setup_memory();
void setup_timer();
uint8_t insertdisk(uint8_t drivenum, size_t size, char *ROM);
void reset86();
void exec86(uint32_t execloops);
uint8_t read86(uint32_t addr32);
void write86(uint32_t addr32, uint8_t value);
void doirq(uint8_t irqnum);
void incsends();
void init_display();
void write_video(uint16_t addr);
void clear_display();
void palettereset();
void display_CSIP();
void ps2poll();
void setup_ps2(uint8_t data_pin, uint8_t irq_pin);
void video_init();
#define VRAM_write(addr32, value) (VRAM[addr32] = value)
#define VRAM_read(addr32) (VRAM[addr32])
void out8253 (uint16_t portnum, uint8_t value);
uint8_t in8253 (uint16_t portnum);
void out8259 (uint16_t portnum, uint8_t value);
uint8_t in8259 (uint16_t portnum);
void init8253();
void init8259();
void net_init();
void net_loop();
void net_handler();
uint8_t cached_read(uint32_t addr32);
void cached_write(uint32_t addr32, uint8_t value);
void cache_init();

extern uint8_t SPI_RAM_pins[8];
extern uint8_t net_mac[6];
extern uint8_t bufSerial[1600];
void outByte(uint8_t cc);

extern struct i8253_s {
    uint16_t chandata[3];
    uint8_t accessmode[3];
    uint8_t bytetoggle[3];
    uint32_t effectivedata[3];
    float chanfreq[3];
    uint8_t active[3];
    uint16_t counter[3];
} i8253;

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


