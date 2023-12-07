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
