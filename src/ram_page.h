#pragma once
#include <stdbool.h>
#include <inttypes.h>

#define RAM_PAGE_SIZE_KB 4ul
#define RAM_PAGE_SIZE (RAM_PAGE_SIZE_KB * 1024)
#define RAM_IN_PAGE_ADDR_MASK (0x00000FFF)

#if PICO_ON_DEVICE
#define RAM_SIZE (4 * 40) // 40 pages 4Kb = 160KB real pico RAM
#else
#define RAM_SIZE (640)
#endif

extern uint8_t RAM[RAM_SIZE << 10];

#if SD_CARD_SWAP

#define RAM_BLOCKS (RAM_SIZE / RAM_PAGE_SIZE_KB)
extern uint16_t RAM_PAGES[RAM_BLOCKS]; // lba (14-0); 15 - written

bool init_vram();

uint8_t ram_page_read(uint32_t addr32);
void ram_page_write(uint32_t addr32, uint8_t value);
void ram_page_write16(uint32_t addr32, uint16_t value);

void read_vram_block(char* dst, uint32_t file_offset, uint32_t sz);
void flush_vram_block(const char* src, uint32_t file_offset, uint32_t sz);

#endif
