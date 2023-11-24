#pragma once
#define PORT_A20 0x0092
#define A20_ENABLE_BIT 0x02

#include <stdbool.h>
#include <inttypes.h>

uint8_t set_a20(uint8_t cond);
bool    get_a20_enabled();


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

// Maximum number of map entries in the e820 map
#define BUILD_MAX_E820 32

// e820 map storage
extern struct e820entry e820_list[];
extern int e820_count;
