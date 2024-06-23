#pragma once
#include <stdbool.h>
#include <inttypes.h>
/* CGA
#define VIDEORAM_START32 0xB8000ul
#define VIDEORAM_END32 0xC8000ul
*/
#define VIDEORAM_START32 0xA0000ul
#define VIDEORAM_END32 0xC0000ul

//#define VIDEORAM_SIZE (VIDEORAM_END32 - VIDEORAM_START32)
#define VIDEORAM_SIZE (64 << 10)

// --- select only one of 'em
#ifdef SWAP_BLOCK_1k
#define RAM_PAGE_SIZE_KB 1ul
#define RAM_IN_PAGE_ADDR_MASK (0x000003FF)
#define SHIFT_AS_DIV 10
#endif
// --- select only one of 'em
#ifdef SWAP_BLOCK_2k
#define RAM_PAGE_SIZE_KB 2ul
#define RAM_IN_PAGE_ADDR_MASK (0x000007FF)
#define SHIFT_AS_DIV 11
#endif
// --- select only one of 'em
#ifdef SWAP_BLOCK_4k
#define RAM_PAGE_SIZE_KB 4ul
#define RAM_IN_PAGE_ADDR_MASK (0x00000FFF)
#define SHIFT_AS_DIV 12
#endif
// --- select only one of 'em ^

#define RAM_PAGE_SIZE (RAM_PAGE_SIZE_KB * 1024)

#define RAM_SIZE (2 * 74ul << 10) // 75 pages (2Kb) = 150KB real pico RAM

extern uint8_t RAM[RAM_SIZE];

#define RAM_BLOCKS (RAM_SIZE / RAM_PAGE_SIZE)
extern uint16_t RAM_PAGES[RAM_BLOCKS]; // lba (14-0); 15 - written

#define CONVENTIONAL_END 0xA0000ul

bool init_vram();


uint8_t ram_page_read(uint32_t addr32);
uint16_t ram_page_read16(uint32_t addr32);

void ram_page_write(uint32_t addr32, uint8_t value);
void ram_page_write16(uint32_t addr32, uint16_t value);
#if !PICO_ON_DEVICE
#define EXT_RAM_SIZE 8 << 20 // 8Mb
void write8psram(uint32_t addr32, uint8_t v);
void write16psram(uint32_t addr32, uint16_t v);
uint8_t read8psram(uint32_t addr32);
uint16_t read16psram(uint32_t addr32);
#endif

void read_vram_block(char* dst, uint32_t file_offset, uint32_t sz);
void flush_vram_block(const char* src, uint32_t file_offset, uint32_t sz);

