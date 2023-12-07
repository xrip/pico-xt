#pragma once

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
