#pragma once
#include <stdbool.h>
#include <inttypes.h>

#define VRAM_START32 0xB8000ul
#define VRAM_END32 0xC8000ul
#define VRAM_SIZE (VRAM_END32 - VRAM_START32)

#define RAM_PAGE_SIZE_KB 4ul
#define RAM_PAGE_SIZE (RAM_PAGE_SIZE_KB * 1024)
#define RAM_IN_PAGE_ADDR_MASK (0x00000FFF)

#if PICO_ON_DEVICE
#define RAM_SIZE (4 * 40ul << 10) // 40 pages 4Kb = 160KB real pico RAM
#else
#define RAM_SIZE (640ul << 10)
#endif

extern uint8_t RAM[RAM_SIZE];

#define RAM_BLOCKS (RAM_SIZE / RAM_PAGE_SIZE)
extern uint16_t RAM_PAGES[RAM_BLOCKS]; // lba (14-0); 15 - written

#define CONVENTIONAL_END 0xA0000ul

// after conventional before VRAM
#define UMB_1_START CONVENTIONAL_END
#define UMB_1_END VRAM_START32
#define UMB_1_SIZE (UMB_1_END - UMB_1_START)

// after VRAM before EMM
#define UMB_2_START VRAM_END32
#define UMB_2_END 0xD0000ul
#define UMB_2_SIZE (UMB_2_END - UMB_2_START)

// after EMM before BASIC
#define UMB_3_START 0xE0000ul
#define UMB_3_END 0xF6000ul
#define UMB_3_SIZE (UMB_3_END - UMB_3_START)

bool init_vram();

uint8_t ram_page_read(uint32_t addr32);
uint16_t ram_page_read16(uint32_t addr32);

void ram_page_write(uint32_t addr32, uint8_t value);
void ram_page_write16(uint32_t addr32, uint16_t value);

void read_vram_block(char* dst, uint32_t file_offset, uint32_t sz);
void flush_vram_block(const char* src, uint32_t file_offset, uint32_t sz);

