#pragma once
#define PORT_A20 0x92
#define A20_ENABLE_BIT 0x02
#define UMB_START_ADDRESS 0xC8000ul
#define UMB_BLOCKS 5
#define HMA_START_ADDRESS 0x100000ul
#define OUT_OF_HMA_ADDRESS 0x10FFF0ul
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

#define E820_RAM          1
#define E820_RESERVED     2
#define E820_ACPI         3
#define E820_NVS          4
#define E820_UNUSABLE     5

struct e820entry {
    uint64_t start;
    uint64_t size;
    uint32_t type;
};

void e820_add(uint64_t start, uint64_t size, uint32_t type);
void e820_remove(uint64_t start, uint64_t size);
void e820_prepboot(void);
void i15_87h(uint16_t words_to_move, uint32_t gdt_far);
void i15_89h(uint8_t IDT1, uint8_t IDT2, uint32_t gdt_far);

// Maximum number of map entries in the e820 map
#define BUILD_MAX_E820 32

// e820 map storage
extern struct e820entry e820_list[];
extern int e820_count;

bool umb_in_use(uint32_t addr32);
extern bool hma_in_use;

uint8_t xms_fn();
void xmm_reboot();
