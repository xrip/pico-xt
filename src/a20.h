#pragma once
#define PORT_A20 0x92
#define A20_ENABLE_BIT 0x02
#define UMB_START_ADDRESS 0xC8000ul
#define UMB_BLOCKS 5
#define HMA_START_ADDRESS 0x100000ul
#define OUT_OF_HMA_ADDRESS 0x10FFF0ul
#define BASE_XMS_HANLES_SEG 0x11000ul
#define XMS_STATIC_PAGE_KBS 16ul
#define XMS_STATIC_PAGE_PHARAGRAPS (XMS_STATIC_PAGE_KBS << 6)
// last byte of interrupts table (actually should not be ever used as CS:IP)
#define XMS_FN_CS 0x0000
#define XMS_FN_IP 0x03FF

#include <stdbool.h>
#include <inttypes.h>
#include "emulator.h"

bool    get_a20_enabled();
void    set_a20_enabled(bool v);
void    set_a20_global_enabled();
void    set_a20_global_diabled();

bool INT_15h();
bool umb_in_use(uint32_t addr32);
extern bool hma_in_use;

uint8_t xms_fn();
void xmm_reboot();
