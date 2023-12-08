#pragma once
// type of 8-bit write function pointer
typedef void (*write_fn_ptr)(uint32_t, uint8_t);
// type of 16-bit write function pointer
typedef void (*write16_fn_ptr)(uint32_t, uint16_t);
// type of 8-bit read function pointer
typedef uint8_t (*read_fn_ptr)(uint32_t);
// type of 16-bit read function pointer
typedef uint16_t (*read16_fn_ptr)(uint32_t);
// replace RAM mapping access function
void update_segment_map(uint16_t seg, write_fn_ptr p8w, write16_fn_ptr p16w, read_fn_ptr p8r, read16_fn_ptr p16r);
// replace HMA mapping access
void update_segment_map_hma(uint8_t ba, write_fn_ptr p8w, write16_fn_ptr p16w, read_fn_ptr p8r, read16_fn_ptr p16r);

static __inline uint16_t read16arr(uint8_t* arr, uint32_t base_addr, uint32_t addr32) {
    arr += addr32 - base_addr;
    register uint16_t b1 = *arr++;
    register uint16_t b0 = *arr;
    return b1 | (b0 << 8);
}

static __inline uint16_t read16arr0(uint8_t* arr, uint32_t addr32) {
    arr += addr32;
    register uint16_t b1 = *arr++;
    register uint16_t b0 = *arr;
    return b1 | (b0 << 8);
}

#if PICO_ON_DEVICE
  #include "psram_spi.h"
  #include "ram_page.h"
#else
  #define EXT_RAM_SIZE 32 << 20 // 32Mb

extern uint8_t EXTRAM[EXT_RAM_SIZE];
#define psram_read8(a, addr32) EXTRAM[addr32]
#define psram_read16(a, addr32) (uint16_t)EXTRAM[addr32]


#define psram_write8(a, addr32, value) EXTRAM[addr32] = value
#define psram_write16(a, addr32, value) EXTRAM[addr32] = value & 0xFF; \
EXTRAM[addr32] = value >> 8;

#define psram_write32(a, addr32, value)

uint8_t ram_page_read(uint32_t addr32) {
    return EXTRAM[addr32];
}

uint16_t ram_page_read16(uint32_t addr32) {
    return (EXTRAM[addr32] + EXTRAM[addr32+1] << 8);
}

void ram_page_write(uint32_t addr32, uint8_t value) {
    EXTRAM[addr32] = value;
}

void ram_page_write16(uint32_t addr32, uint16_t value) {
    EXTRAM[addr32] = value & 0xFF;
    EXTRAM[addr32] = value >> 8;
}

#define init_vram() 1
#define psram_cleanup() 1

static uint8_t read8psram(uint32_t addr32) {
    return EXTRAM[addr32];
}

static uint16_t read16psram(uint32_t addr32) {
    return (EXTRAM[addr32] + EXTRAM[addr32+1] << 8);
}

static void write8psram(uint32_t addr32, uint8_t value) {
    EXTRAM[addr32] = value;
}
static void write16psram(uint32_t addr32, uint16_t value) {
    EXTRAM[addr32] = value & 0xFF;
    EXTRAM[addr32] = value >> 8;
}
#endif
extern bool PSRAM_AVAILABLE;
