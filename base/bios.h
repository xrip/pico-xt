#pragma once
#ifndef BIOS_H
#define BIOS_H

#include <mem.h>
#include <conio.h>
#include "stdbool.h"

#include "dos3_3.h"
//#include "freedos.h"
#include "cpu8086.h"
#include "disk.h"
#include "ports.h"

int cursor_x = 0;
int cursor_y = 0;
static int color = 7;
static int do_not_IRET;

uint8_t VRAM[VRAM_SIZE << 10];


bool bios_started = false;
#define INTERNAL_BIOS_TRAP_SEG 0xF000
#define BIOS_TRAP_EMUGW    0x100
#define BIOS_TRAP_RESET    0x101
#define BIOS_TRAP_BASIC    0x102
#define BIOS_TRAP_HALT    0x103

#define pokeb(a, b) RAM[a]=(b)
#define peekb(a)   RAM[a]

static inline void pokew(int a, uint16_t w) {
    pokeb(a, w & 0xFF);
    pokeb(a + 1, w >> 8);
}

static inline uint16_t peekw(int a) {
    return peekb(a) + (peekb(a + 1) << 8);
}

static void bios_putchar(const char c) {
    //printf("\033[%im%c", color, c);
    if (c == 0x0D) {
        cursor_x = 0;
        cursor_y++;
    } else if (c == 0x0A) {
        cursor_x = 0;
    } else if (c == 0x08 && cursor_x > 0) {
        cursor_x--;
        VRAM[/*0xB8000 + */(cursor_y * 160) + cursor_x * 2 + 0] = 32;
        VRAM[/*0xB8000 + */(cursor_y * 160) + cursor_x * 2 + 1] = color;
    } else {
        VRAM[/*0xB8000 + */(cursor_y * 160) + cursor_x * 2 + 0] = c & 0xFF;
        VRAM[/*0xB8000 + */(cursor_y * 160) + cursor_x * 2 + 1] = color;
        if (cursor_x == 79) {
            cursor_x = 0;
            cursor_y++;
        } else
            cursor_x++;
    }

    if (cursor_y == 25) {
        cursor_y = 24;

        memmove(VRAM/* + 0xB8000*/, VRAM /*+ 0xB8000*/ + 160, 80 * 25 * 2);
        for (int a = 0; a < 80; a++) {
            VRAM[/*0xB8000 + */24 * 160 + a * 2 + 0] = 32;
            VRAM[/*0xB8000 + */24 * 160 + a * 2 + 1] = color;

        }
    }
}

static void bios_putstr(const char *s) {
    while (*s)
        bios_putchar(*s++);
}

#define bios_printf(...)                    \
    do {                            \
        char _buf_[4096];                \
        snprintf(_buf_, sizeof(_buf_), __VA_ARGS__);    \
        bios_putstr(_buf_);                \
    } while (0)


// INT 19h
static void bios_boot_interrupt ( void )
{
    if (!disk[bootdrive].inserted) {
        fprintf(stderr, "BIOS: ERROR! Requested boot drive %02Xh is not inserted!", bootdrive);
        return;
    }

    bios_printf("\nBooting from drive %02Xh ... ", bootdrive);
    bios_read_boot_sector(bootdrive, 0, 0x7C00);	// read MBR!
    if (CPU_FL_CF) {
        fprintf(stderr, "BIOS: ERROR! System cannot boot (cannot read boot record)! AH=%02Xh\n", CPU_AH);
        return;
    }
    bios_putstr("OK.\n\n");
    // Initialize environment needed for the read OS boot loader.
    CPU_SP = 0x400;		// set some value to SP, this seems to be common in BIOSes
    CPU_CS = 0;
    CPU_IP = 0x7C00;	// 0000:7C000 where we loaded boot record and what BIOSes do in general
    CPU_SS = 0;
    CPU_DS = 0;
    CPU_ES = 0;
    CPU_DX = bootdrive;	// DL only, but some BIOSes passes DH=0 too ...
    decodeflagsword(0);	// clear all flags just to be safe
    do_not_IRET = 1;
}

void bios_reset() {
    memset(RAM, 0, 1024 + 256);    // clear some part of the main RAM to be sure

    pokew(0x1C * 4, 0x1FF);            // override, this interrupt points to an IRET
    // Configuration word. This word is returned by INT 11h
    pokew(0x410,
          ((fdcount ? 1 : 0) << 0) +    // bit 0: "1" if one of more floppy drives is in the system
          ((0) << 1) +            // bit 1: XT not used, AT -> 1 if there is FPU
          ((0) << 2) +            // bit 2-3: XT = memory on motherboard (max 64K), AT = not used
          ((0) << 4) +
          // bit 4-5: video mode on startup: 0=VGA/EGA, 1=40*25 colour, 2=80*25 colour, 3=80*25 mono
          ((fdcount ? fdcount - 1 : 0) << 6) +    // bit 6-7: floppy drives in system (!! 0 means ONE, etc)
          ((0) << 8) +            // bit 8: XT = zero if there is DMA, AT = not used
          // MOUSE
          ((0) << 9) +            // bit 9-11: number of RS232 ports
          ((1) << 12) +            // bit 12: XT 1 if game adapter presents, AT = not used
          // bit 13 is not used
          ((1) << 14)            // bit 14-15: number of printers? (probably LPT ports in system)
    );

    pokeb(0x412, 1);        // POST interrupt flag? no idea, but it seems some BIOS at least sets this to one
    pokeb(0x440, 0x26);        // floppy timeout, well just to have similar value here as usual BIOS does
    pokeb(0x449, 3);        // active video mode setting?
    pokew(0x44C, 0x1000);        // size of active video in page bytes ...
    pokew(0x400, 0x3F8); // COM1 (first COM port) base I/O address
    pokew(0x408,
          0x3BC);        // I/O base for LPT ... we don't need this, but maybe some software goes crazy trying to interpet otherwise zero here as a valid port and output there ...
    pokew(0x413,
          RAM_SIZE);        // base memory (below 640K!), returned by int12h. ... 640Kbyte ~ "should be enough for everyone" - remember?

    pokeb(0x475, hdcount);        // number of HDDs in the system
    pokew(0x44A, 80);        // WORD: Number of textcolumns per row for the active video mode
    pokeb(0x484, 24);        // BYTE: Number of video rows - 1
    pokew(0x463,
          0x3D4);        // WORD: I/O port of video display adapter, _OR_ the segment address of video RAM?!?!??!?!
    pokeb(0x465, 0x29);        // Video display adapter internal mode register
    pokeb(0x466, 0x30);        // colour palette??
    pokew(0x467, 3);        // adapter ROM offset
    pokew(0x469, 0xC000);        // adapter ROM segment
    pokew(0x4A8, 0x00A1);        // video parameter block offset
    pokew(0x4AA, 0xC000);        // video parameter block segment
    pokew(0x472, 0x1234);        // POST soft reset flag?
    // Init keyboard buffer vars
    pokew(0x41A, 0x1E);
    pokew(0x41C, 0x1E);
    pokew(0x480, 0x1E);
    pokew(0x482, 0x1E + 0x20);

    color = 0x4E;
    bios_putstr("Internal BIOS v0.01\n");
    color = 7;
    bios_printf("CPU type : %s\nMemory   : %dKb\r\n", "8086", RAM[0x413]);
    bios_started = true;

    insertdisk(0, sizeof FD0, FD0);
    //bios_boot_interrupt();
}


// Int 0x10
void videoBIOSinterupt() {
    //printf("INT 10h CPU_AH: 0x%x CPU_AL: 0x%x\r\n", CPU_AH, CPU_AL);

    switch (CPU_AH) {
        case 0x00:
            videomode = CPU_AL;
            printf("VBIOS: Mode 0x%x\r\n", CPU_AX);
            // Установить видеорежим
            break;
        case 0x01:
            // TODO!!: Сделать мигание курсора
            break;
        case 0x02: // Установить позицию курсора
            cursor_x = CPU_DL;
            cursor_y = CPU_DH;
            break;
        case 0x03: // Получить позицию курсора
            CPU_DL = cursor_x;
            CPU_DH = cursor_y;
            break;
        case 0x05: //   INT 10h,  05h (5)        Set Active Display Page
            printf("INT 10h,  05h (5)        Set Active Display Page: %i\r\n", CPU_AL);
            break;
        case 0x06: // INT 10h,  06h (6)        Scroll Window Up
            printf("INT 10h,  06h (6)        Scroll Window Up\r\n");
            /*
            printf(
                   "                      AL %i         Number of lines to scroll (if 0, clear entire window)\n"
                   "                      BH %i         Display attribute for blank lines\n"
                   "                      CH %i         Row number of upper left corner\n"
                   "                      CL %i         Column number of upper left corner\n"
                   "                      DH %i         Row number of lower right corner\n"
                   "                      DL %i         Column number of lower right corner\n", CPU_AL, CPU_BH, CPU_CH, CPU_CL, CPU_DH, CPU_DL);
            */
             if (!CPU_AL) {
                // FIXME!! Нормально сделай!
                memset(VRAM, 0x00, 160*25);
                break;
            }
            break;
        case 0x08: // Получим чар под курсором
                CPU_AL = VRAM[(cursor_y * 160 + cursor_x * 2) + 0];
                CPU_AH = VRAM[(cursor_y * 160 + cursor_x * 2) + 1];
            break;
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

            break;
        case 0x0E:
            /*0eH писать символ на активную видео страницу (эмуляция телетайпа)
              вход:  AL = записываемый символ (использует существующий атрибут)
              BL = цвет переднего плана (для графических режимов)*/
            bios_putchar(CPU_AL);
            break;
            default:
              printf("Undefined videoBIOS interupt 0x%x\r\n", CPU_AH);
    }
}




// Int 21h
void DOSinterupt() {
    uint16_t adrs;
    switch (regs.byteregs[regah]) {
        case 0x01:
            /*Вход AH = 01H
            Выход AL = символ, полученный из стандартного ввода
            Считывает (ожидает) символ со стандартного входного устройства.
             Отображает этот символ на стандартное выходное устройство (эхо)*/

            regs.byteregs[regal] = (uint8_t) getch();
            bios_putchar(regs.byteregs[regal]);
            break;
        case 0x02:
            bios_putchar(regs.byteregs[regdl]);
            break;
        case 0x09:
            // AH=09h - вывод строки из DS:DX.
            for (uint8_t i = 0; i < 255; i++) {
                char ch = (char) read86((segregs[regds] << 4) + regs.wordregs[regdx] + i);
                if (ch != '$')
                    bios_putchar(ch);
                    //Serial.print(ch);
                else {
                    regs.byteregs[regal] = 0x24;
                    return;
                }
            }
            break;
        case 0x0a:
            /*DOS Fn 0aH: ввод строки в буфеp
            Вход AH = 0aH
            DS:DX = адрес входного буфера (смотри ниже)
            Выход нет = буфер содержит ввод, заканчивающийся символом CR (ASCII 0dH)*/
            adrs = (segregs[regds] << 4) + regs.wordregs[regdx];
            uint8_t length = 0;
            char ch;
            while (true) {
                ch = (char) getch();
                bios_putchar(ch);
                if (ch == '\n' || ch == '\r')
                    break;
                write86(adrs + length + 2, (uint8_t) ch);
                length++;
                if (length > read86(adrs) || length > 255)
                    break;
            }
            write86(adrs + 1, length);//записываем действительную длину данных
            write86(adrs + length + 3, '$');
            break;
#ifdef DEBUG
            default:
              Serial.print("undefined DOS interupt ");
              Serial.print(regs.byteregs[regah],HEX);
#endif
    }
}

// This is the IRQ handler for the periodic interrupt of BIOS
static void bios_irq0_handler(void) {
    // Increment the BIOS counter.
    uint32_t cnt = (peekw(0x46C) + (peekw(0x46E) << 16)) + 1;
    pokew(0x46C, cnt & 0xFFFF);
    pokew(0x46E, cnt >> 16);
    //puts("BIOS: IRQ0!\r\n");
    // FIXME!!
    portout(0x20, 0x20);	// send end-of-interrupt command to the interrupt controller
}

static void kbd_set_mod0 ( int mask, int scan )
{
    if ((scan & 0x80))
        RAM[0x417] &= ~mask;
    else
        RAM[0x417] |= mask;
}
static const uint8_t scan2ascii[] = {
        //0    1     2      3     4     5     6     7     8    9     A      B     C    D     E     F
        0x00, 0x1B, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30, 0x2D, 0x3D, 0x08, 0x09,
        0x71, 0x77, 0x65, 0x72, 0x74, 0x79, 0x75, 0x69, 0x6F, 0x70, 0x5B, 0x5D, 0x0D, 0x00, 0x61, 0x73,
        0x64, 0x66, 0x67, 0x68, 0x6A, 0x6B, 0x6C, 0x3B, 0x27, 0x60, 0x00, 0x5C, 0x7A, 0x78, 0x63, 0x76,
        0x62, 0x6E, 0x6D, 0x2C, 0x2E, 0x2F, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x7F, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2A, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x04, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1C, 0x00,


        0x00, 0x37, 0x2E, 0x20, 0x2F, 0x30, 0x31, 0x21, 0x32, 0x33, 0x34, 0x35, 0x22, 0x36, 0x38, 0x3E,
        0x11, 0x17, 0x05, 0x12,	0x14, 0x19, 0x15, 0x09,	0x0F, 0x10, 0x39, 0x3A,	0x3B, 0x84, 0x61, 0x13,
        0x04, 0x06, 0x07, 0x08,	0x0A, 0x0B, 0x0C, 0x3F,	0x40, 0x41, 0x82, 0x3C,	0x1A, 0x18, 0x03, 0x16,
        0x02, 0x0E, 0x0D, 0x42,	0x43, 0x44, 0x81, 0x3D,	0x88, 0x2D, 0xC0, 0x23,	0x24, 0x25, 0x26, 0x27,
        0x28, 0x29, 0x2A, 0x2B,	0x2C, 0xA0, 0x90
#if 0
        0x00, 0x37, 0x2E, 0x20, 0x2F, 0x30, 0x31, 0x21, 0x32, 0x33, 0x34, 0x35, 0x22, 0x36, 0x38, 0x3E,
	0x11, 0x17, 0x05, 0x12,	0x14, 0x19, 0x15, 0x09,	0x0F, 0x10, 0x39, 0x3A,	0x3B, 0x84, 0x01, 0x13,
	0x04, 0x06, 0x07, 0x08,	0x0A, 0x0B, 0x0C, 0x3F,	0x40, 0x41, 0x82, 0x3C,	0x1A, 0x18, 0x03, 0x16,
	0x02, 0x0E, 0x0D, 0x42,	0x43, 0x44, 0x81, 0x3D,	0x88, 0x2D, 0xC0, 0x23,	0x24, 0x25, 0x26, 0x27,
	0x28, 0x29, 0x2A, 0x2B,	0x2C, 0xA0, 0x90
#endif
};
static int kbd_push_buffer ( uint16_t data )
{
    uint16_t buftail = peekw(0x41C);
    uint16_t buftail_adv = buftail + 2;
    if (buftail_adv == peekw(0x482))
        buftail_adv = peekw(0x480);
    if (buftail_adv == peekw(0x41A))
        return 1;		// buffer is full!
    pokew(0x400 + buftail, data);
    pokew(0x41C, buftail_adv);
    return 0;
}

static uint16_t kbd_get_buffer ( int to_remove )
{
    uint16_t bufhead = peekw(0x41A);
    if (bufhead == peekw(0x41C))
        return 0;	// no character is available in the buffer
    uint16_t data = peekw(0x400 + bufhead);
    if (!to_remove)
        return data;
    bufhead += 2;
    if (bufhead == peekw(0x482))
        pokew(0x41A, peekw(0x480));
    else
        pokew(0x41A, bufhead);
    return data;
}


// Interrupt 16h: keyboard functions
void keyBIOSinterupt() {
    printf("INT 16h CPU_AH: 0x%x CPU_AL: 0x%x\r\n", CPU_AH, CPU_AL);
    switch (regs.byteregs[regah]) {
        case 0x00:
            /*00H читать (ожидать) следующую нажатую клавишу
            выход: AL = ASCII символ (если AL=0, AH содержит расширенный код ASCII )
                  AH = сканкод  или расширенный код ASCII*/
            CPU_AX = getch(); //kbd_get_buffer(1);

            break;
        case 0x01:
            CPU_AX = getch(); // kbd_get_buffer(0);
            if (CPU_AX)
                CPU_FL_ZF = 0;
            else
                CPU_FL_ZF = 1;
            break;
        case 0x02:
            CPU_AL = RAM[0x417];
            break;
        case 5:				// PUSH into kbd buffer by user call!!
            CPU_AL = kbd_push_buffer(CPU_CX);
            break;
        default:
            printf("BIOS: unknown 16h interrupt function %02Xh\n", CPU_AH);
            CPU_FL_CF = 1;
            CPU_AH = 1;
            break;
#ifdef DEBUG
            default:
              Serial.print("undefined keyBIOS interupt ");
              Serial.print(regs.byteregs[regah],HEX);
#endif
    }
}

// This is the IRQ handler for the keyboard interrupt of BIOS
static void bios_irq1_handler(void) {
    // keyboard
    //puts("BIOS: IRQ1!\r\n");
    /* FIXME!! */
   //	if ((portram[0x64] & 2)) {	// is input buffer full
       //uint8_t scan = portram[0x60];	// read the scancode
       uint8_t scan = portin(0x60);
       uint8_t ctrlst = portin(0x61);
       portout(0x61, ctrlst | 0x80);
       portout(0x61, ctrlst);
       //portram[0x64] &= ~2;		// empty the buffer
       //printf("BIOS: scan got: %Xh\n", scan);
       switch (scan & 0x7F) {
           case 0x36: kbd_set_mod0(0x01, scan); break;	// rshift
           case 0x2A: kbd_set_mod0(0x02, scan); break;	// lshift
           case 0x1D: kbd_set_mod0(0x04, scan); break;	// ctrl
           case 0x38: kbd_set_mod0(0x08, scan); break;	// alt
           case 0x46: kbd_set_mod0(0x10, scan); break;	// scroll lock
           case 0x45: kbd_set_mod0(0x20, scan); break;	// num lock
           case 0x3A: kbd_set_mod0(0x40, scan); break;	// caps lock
           case 0x52: kbd_set_mod0(0x80, scan); break;	// ins
           default:
               if (scan < 0x80) {
                   uint8_t ascii;
                   if (scan <= sizeof(scan2ascii))
                       ascii = scan2ascii[scan];
                   else
                       ascii = 0;
                   if ((RAM[0x417] & 3)) {
                       if (ascii == ';')
                           ascii = ':';
                   }
                   kbd_push_buffer(ascii | (scan << 8));
               }
               break;
       }
   //	}
       portout(0x20, 0x20);	// send end-of-interrupt command to the interrupt controller
       /* */
}


static void bios_internal_trap(unsigned int trap) {
    printf("bios_internal_trap 0x%x !! \r\n", trap);
    int do_override_some_flags = 1;
    // get return CS:IP from stack WITHOUT POP'ing them!
    // this are used only for debug purposes! [know the top element of our stack frame @ x86 level]
    uint16_t stack_ip = peekw(CPU_SS * 16 + CPU_SP);
    uint16_t stack_cs = peekw(CPU_SS * 16 + ((CPU_SP + 2) & 0xFFFF));
    do_not_IRET = 0;
    if (trap < 0x100)
        CPU_FL_CF = 0;    // by default we set carry flag to zero. INT handlers may set it '1' in case of error!
    //printf("BIOS_TRAP: %04Xh STACK_RET=%04X:%04X AX=%04Xh\n", trap, return_segment, return_offset, CPU_AX);
    switch (trap) {
        case 0x00:
            bios_putstr("Division by zero.\n");
            do_override_some_flags = 0;
            break;
        case 0x08:
            bios_irq0_handler();
            do_override_some_flags = 0;
            break;
        case 0x09:
            bios_irq1_handler();
            do_override_some_flags = 0;
            break;
        case 0x10:        // Interrupt 10h: video services
            // FIXME! Some time video.c (vidinterrupt function) and these things must be unified here!
            // the problem: in non-internal BIOS mode, Fake86 uses ugly hacks in cpu.c involving vidinterrupt() directly.
            switch (CPU_AH) {
                case 0x0E:
                    bios_putchar(CPU_AL);
                    break;
                default:
                    videoBIOSinterupt();
                    //printf("BIOS: unknown 10h interrupt function %02Xh\n", CPU_AH);
                    //CPU_FL_CF = 1;
                    //CPU_AH = 1;
                    break;
            }
            break;
        case 0x11:        // Interrupt 11h: get system configuration
            printf("INT ?!!! 0x11 \r\n");
            CPU_AX = peekw(0x410);
            break;
        case 0x12:        // Interrupt 12h: get memory size in Kbytes
            //CPU_AX = RAM[0x413] + (RAM[0x414] << 8);
            CPU_AX = peekw(0x413);
            printf("BIOS: int 12h answer (base RAM size), AX=%d\r\n", CPU_AX);
            break;
        case 0x13:        // Interrupt 13h: disk services
            diskhandler();
            break;
        case 0x14:        // Interrupt 14h: serial stuffs
            switch (CPU_AH) {
                case 0x00:    // Serial - initialize port
                    printf("BIOS: int 14h serial initialization request for port %u\r\n", CPU_DX);
                    if (CPU_DX == 0) {
                        CPU_AH = 64 + 32;    // tx buffer and shift reg is empty
                        CPU_AL = 0;
                    } else {
                        CPU_AH = 128;    // timeout
                        CPU_AL = 0;
                    }
                    break;
                default:
                    printf("BIOS: unknown 14h interrupt function %02Xh\r\n", CPU_AH);
                    CPU_FL_CF = 1;
                    CPU_AH = 1;
                    break;
            }
            break;
        case 0x15:
            CPU_FL_CF = 1;
            CPU_AH = 0x86;
            printf("BIOS: unknown 15h AT ALL interrupt function, AX=%04Xh\r\n", CPU_AX);
            break;
        case 0x16:        // Interrupt 16h: keyboard functions TODO+FIXME !
            keyBIOSinterupt();
            break;
        case 0x1A:        // Interrupt 1Ah: time services
            switch (CPU_AH) {
                case 0x00:    // get 18.2 ticks/sec since midnight and day change flag
                printf("TIMER?\r\n");
                    CPU_DX = peekw(0x46C);
                    CPU_CX = peekw(0x46E);
                    CPU_AL = peekb(0x470);
                    break;
                case 0x01:
                    pokew(0x46C, CPU_DX);
                    pokew(0x46E, CPU_CX);
                    break;
                case 0x02:    // read real-time clock
                {
//                    time_t uts = time(NULL);
//                    struct tm *t = localtime(&uts);
                    CPU_DH = 00;//t->tm_sec;
                    CPU_CL = 59; //t->tm_min;
                    CPU_CH = 23; //t->tm_hour;
                    CPU_DL = 0; //t->tm_isdst > 0;
                    printf("BIOS: RTC time requested, answer: %02u:%02u:%02u DST=%u\r\n", CPU_CH, CPU_CL, CPU_DH, CPU_DL);
                }
                    CPU_FL_CF = 0;
                    break;
                case 0x04:    // read real-time clock's date
                {
                    //time_t uts = time(NULL);
                    //struct tm *t = localtime(&uts);
                    CPU_DL = 22; //t->tm_mday;
                    CPU_DH = 10; //-t->tm_mon + 1;
                    CPU_CL = 2023 % 100;
                    CPU_CH = 2023 / 100 + 19;
                    printf("BIOS: RTC date requested, answer: %02u%02u.%02u.%02u\r\n", CPU_CH, CPU_CL, CPU_DH, CPU_DL);
                }
                    CPU_FL_CF = 0;
                    break;
                default:
                    printf("BIOS: unknown 1Ah interrupt function %02Xh\n", CPU_AH);
                    CPU_FL_CF = 1;
                    CPU_AH = 1;
                    break;
            }
            break;
        case 0xE6:    // "Filesystem server" invented by Mach, allow to map host FS to DOS drives!
            switch (CPU_AH) {
                case 0x00:    // Installation check
                    CPU_AX = 0xAA55;    // magic number to return
                    CPU_BX = 0x0101;    // high/low byte: major/minor version number
                    CPU_CX = 0x0001;    // patch level
                    break;
                case 0x01:    // Register dump
                    puts("HOSTFS: register dump: TODO\r\n");
                    break;
                case 0xFF:
                    puts("HOSTFS: requested terminate\r\n");
                    break;
            }
            break;
        case BIOS_TRAP_RESET:
            printf("BIOS_TRAP_RESET\r\n");
            bios_reset();
            //return_segment = 0;
            //return_offset = 0x7C00;
            //return_flags = 0;
            //printf("BIOS: will return to %04X:%04X\n", return_segment, return_offset);
            //for (int a = 0; a < 0x200; a++)
            //	RAM[0xB8000 + a ] = a;
            break;
        case 0x18:        // ROM BASIC interrupt :)
        case BIOS_TRAP_BASIC:    // the entry point of ROM basic.
            bios_putstr("No ROM-BASIC. System halted.\r\n");
            do_not_IRET = 1;
            CPU_CS = INTERNAL_BIOS_TRAP_SEG;
            CPU_IP = BIOS_TRAP_HALT;
            break;
        case BIOS_TRAP_HALT:
            do_not_IRET = 1;
            CPU_CS = INTERNAL_BIOS_TRAP_SEG;
            CPU_IP = BIOS_TRAP_HALT;
            break;
        case BIOS_TRAP_EMUGW:
            // emulation gateway functionality can be used by special tools running inside Fake86
            do_not_IRET = 1;
            // do our FAR-RET here instead!
            CPU_IP = cpu_pop();
            CPU_CS = cpu_pop();
            break;
        default:
            if (trap < 0x100) {
                printf("BIOS: unhandled interrupt %02Xh (AX=%04Xh) at %04X:%04X\n", trap, CPU_AX, stack_cs, stack_ip);
                CPU_FL_CF = 1;
                CPU_AH = 1;    // error code?
            } else {
                fprintf(stderr, "BIOS: FATAL: invalid trap number %04Xh (stack frame: %04X:%04X)\n", trap, stack_cs,
                        stack_ip);
                //exit(1);
            }
            break;
    }
    if (!do_not_IRET) {
        printf("Simulate an IRET by our own\r\n");
        int zf_to_set = CPU_FL_ZF;
        int cf_to_set = CPU_FL_CF;
        // Simulate an IRET by our own
        CPU_IP = cpu_pop();
        CPU_CS = cpu_pop();
        decodeflagsword(cpu_pop());
        // Override some flags, if needed
        if (do_override_some_flags) {
            CPU_FL_ZF = zf_to_set;
            CPU_FL_CF = cf_to_set;
        }
    } else {
        printf("BIOS: returning from trap without IRET (trap=%Xh)!\n", trap);
    }
}

int cpu_hlt_handler(void) {
    puts("BIOS: critical warning, HLT outside of trap area?!\r\n");
    return 1;    // Yes, it was really a halt, since it does not fit into our trap area

    if (CPU_CS != INTERNAL_BIOS_TRAP_SEG || saveip >= 0x1FF) {
        puts("BIOS: critical warning, HLT outside of trap area?!\r\n");
        return 1;    // Yes, it was really a halt, since it does not fit into our trap area
    }

    bios_internal_trap(saveip);
    return 0;    // no, it wasn't a HLT, it's our trap!
}

#endif