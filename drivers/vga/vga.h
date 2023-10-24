// ****************************************************************************
//
//                                 VGA output
//
// file derived from the PicoVGA project
// https://github.com/Panda381/PicoVGA
// by Miroslav Nemecek
//
// ****************************************************************************

#ifndef _VGA_H
#define _VGA_H

#include "include.h"

#define DBUF_MAX 640


#ifdef __cplusplus
extern "C" {
#endif

typedef struct  {
  ALIGNED u32 row;
  ALIGNED u32 frame;
  ALIGNED u8 line[DBUF_MAX]; // scanline 1 image data
} VgaLineBuf;

// scanline type
#define LINE_VSYNC	0	// long vertical sync
#define LINE_DARK	1	// dark line
#define LINE_IMG	2	// progressive image 0, 1, 2,...

extern u8 ScanlineType[MAXLINE];

extern sVmode CurVmode; // copy of current videomode table
extern volatile int ScanLine;	// current scan line 1...
extern volatile u32 Frame;	// frame counter
extern volatile int BufInx;	// current buffer set (0..1)
extern volatile Bool VSync;	// current scan line is vsync or dark

// line buffers
extern u32	LineBufHsBp[4];  // HSYNC ... back porch-1 ... IRQ command ... image command
extern u32	LineBufFp;       // front porch+1
extern u32	LineBufDark[2];  // HSYNC ... dark line
extern u32	LineBufSync[10]; // vertical synchronization

// control buffers
extern u32	CtrlBuf1[CBUF_MAX];	// control pairs: u32 count, read address (must be terminated with [0,0])
extern u32	CtrlBuf2[CBUF_MAX];	// control pairs: u32 count, read address (must be terminated with [0,0])

// initialize scanline type table
void ScanlineTypeInit(const sVmode* v);

// initialize videomode (returns False on bad configuration)
void VgaInit(const sVmode* vmode, int width, int height); 

// wait for VSync scanline
void WaitVSync();

VgaLineBuf * __not_in_flash_func(get_vga_line)();




#ifdef __cplusplus
}
#endif

#endif // _VGA_H
