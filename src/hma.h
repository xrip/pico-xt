#pragma once
#include <stdbool.h>
#include <inttypes.h>

#define HMA_START_ADDRESS 0x100000ul
#define BASE_XMS_ADDR     0x110000ul

void   map_hma_ram_pages();
void unmap_hma_ram_pages();