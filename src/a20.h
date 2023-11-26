#pragma once
#define PORT_A20 0x92
#define A20_ENABLE_BIT 0x02

#include <stdbool.h>
#include <inttypes.h>

bool    get_a20_enabled();
void    set_a20_enabled(bool v);

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
