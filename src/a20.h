#pragma once
#include "umb.h"

#define PORT_A20 0x92
#define A20_ENABLE_BIT 0x02

#define HMA_START_ADDRESS 0x100000ul
#define OUT_OF_HMA_ADDRESS 0x10FFF0ul
#define BASE_XMS_HANLES_SEG 0x11000ul
#define BASE_XMS_ADDR 0x110000ul

// TODO:
#if XMS_OVER_HMA_KB
#define XMS_STATIC_PAGE_KBS 16ul
#else
#define XMS_STATIC_PAGE_KBS 0ul
#endif
#define XMS_STATIC_PAGE_PHARAGRAPS (XMS_STATIC_PAGE_KBS << 6)
// last byte of interrupts table (actually should not be ever used as CS:IP)
#define XMS_FN_CS 0x0000
#define XMS_FN_IP 0x03FF

#include <stdbool.h>
#include <inttypes.h>
#include "emulator.h"

#ifdef XMS_DRIVER

bool INT_15h();

#ifdef XMS_HMA
 #define XMS_HMA_KB 64ul
#else
 #define XMS_HMA_KB 0
#endif

uint8_t xms_fn();
void xmm_reboot();
#endif
