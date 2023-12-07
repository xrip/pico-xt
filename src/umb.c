#include "umb.h"
#include "ram_page.h"
#include "ram.h"

#ifdef XMS_UMB
typedef struct umb {
    uint16_t seg;
    uint16_t sz; // paragraphs
    bool allocated;
} umb_t;

static umb_t umb_blocks[UMB_BLOCKS] = {
    UMB_START_ADDRESS >> 4, 0x0800, false,
    0xC800, 0x0800, false,
    0xE000, 0x0800, false,
    0xE800, 0x0800, false,
    0xF000, 0x0800, false,
   // 0xF800, 0x0600, false // 24k before BIOS
    // TODO: over BASIC
};

void init_umb() {
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(p->allocated) {
            p->allocated = false;
            update_segment_map(p->seg, 0, 0, 0, 0);
        }
    }
}

static void write8umb_psram(uint32_t addr32, uint8_t v) {
    if (addr32 <= 0xFE000UL) {
        write8psram(addr32, v);
    }
}

static void write8umb_swap(uint32_t addr32, uint8_t v) {
    if (addr32 <= 0xFE000UL) {
        ram_page_write(addr32, v);
    }
}
static void write16umb_psram(uint32_t addr32, uint16_t v) {
    if (addr32 <= 0xFE000UL) {
        write16psram(addr32, v);
    }
}

static inline uint8_t read86rom(uint32_t addr32) {
    if ((addr32 >= 0xFE000UL) && (addr32 <= 0xFFFFFUL)) {
        // BIOS ROM range
        return *(pBIOS() + addr32 - 0xFE000UL);
    }
    if ((addr32 >= 0xF6000UL) && (addr32 < 0xFA000UL)) {
        // IBM BASIC ROM LOW
        return *(pBASICL + addr32 - 0xF6000UL);
    }
    if ((addr32 >= 0xFA000UL) && (addr32 < 0xFE000UL)) {
        // IBM BASIC ROM HIGH
        return *(pBASICH + addr32 - 0xFA000UL);
    }
    return 0;
}

static void write16umb_swap(uint32_t addr32, uint16_t v) {
    if (addr32 <= 0xFE000UL) {
        ram_page_write16(addr32, v);
    }
}

static uint8_t read8umb_psram(uint32_t addr32) {
    if (addr32 <= 0xFE000UL) {
        return read8psram(addr32);
    }
    return read86rom(addr32);
}

static uint8_t read8umb_swap(uint32_t addr32) {
    if (addr32 <= 0xFE000UL) {
        return ram_page_read(addr32);
    }
    return read86rom(addr32);
}

static inline uint16_t read86rom16(uint32_t addr32) {
    if (addr32 >= 0xFE000UL && addr32 <= 0xFFFFFUL) {
        // BIOS ROM range
        return read16arr(pBIOS(), 0xFE000UL, addr32);
    }
    if (addr32 >= 0xF6000UL && addr32 < 0xFA000UL) {
        // IBM BASIC ROM LOW
        return read16arr(pBASICL(), 0xF6000U, addr32);
    }
    if ((addr32 >= 0xFA000UL) && (addr32 < 0xFE000UL)) {
        // IBM BASIC ROM HIGH
        return read16arr(pBASICH(), 0xFA000UL, addr32);
    }
    return 0;
}

static uint16_t read16umb_psram(uint32_t addr32) {
    if (addr32 <= 0xFE000UL) {
        return read16psram(addr32);
    }
    return read86rom16(addr32);
}

static uint16_t read16umb_swap(uint32_t addr32) {
    if (addr32 <= 0xFE000UL) {
        return ram_page_read16(addr32);
    }
    return read86rom16(addr32);
}

uint16_t umb_allocate(uint16_t* psz, uint16_t* err) {
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(!p->allocated && p->sz >= *psz) {
            p->allocated = true;
            if (p->sz == 0x0800) {
                update_segment_map(
                    p->seg,
                    PSRAM_AVAILABLE ? write8psram  : ram_page_write  ,
                    PSRAM_AVAILABLE ? write16psram : write16umb_swap,
                    PSRAM_AVAILABLE ? read8psram   : ram_page_read   ,
                    PSRAM_AVAILABLE ? read16psram  : ram_page_read16
                );
            } else {
                update_segment_map(
                    p->seg,
                    PSRAM_AVAILABLE ? write8umb_psram  : write8umb_swap  ,
                    PSRAM_AVAILABLE ? write16umb_psram : ram_page_write16,
                    PSRAM_AVAILABLE ? read8umb_psram   : read8umb_swap   ,
                    PSRAM_AVAILABLE ? read16umb_psram  : read16umb_swap
                );
            }
            *psz = p->sz;
            *err = XMS_SUCCESS_CODE;
            return p->seg;
        }
    }
    uint16_t max_sz = 0;
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(!p->allocated && p->sz > max_sz) {
            max_sz = p->sz;
        }
    }
    *psz = max_sz;
    *err = XMS_ERROR_CODE;
    return max_sz ? 0x00B0 /*smaller exists*/ : 0x00B1; // no UMB available
}

uint16_t umb_deallocate(uint16_t* seg, uint16_t* err) {
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(p->allocated && p->seg >= *seg) {
            p->allocated = false;
            update_segment_map(p->seg, 0, 0, 0, 0);
            *err = XMS_SUCCESS_CODE;
            return 0x0000;
        }
    }
    *err = XMS_ERROR_CODE;
    return 0x00B2; // invalid seg
}
#endif
