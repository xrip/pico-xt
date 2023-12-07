#include "umb.h"

#ifdef XMS_UMB
typedef struct umb {
    uint16_t seg;
    uint16_t sz; // paragraphs
    bool allocated;
} umb_t;

static umb_t umb_blocks[UMB_BLOCKS] = {
    UMB_START_ADDRESS >> 4, 0x0800, false,
    0xC800, 0x0800, false,
 //   0xE000, 0x0800, false, // TODO: Adjust UMB issues
 //   0xE800, 0x0800, false,
 //   0xF000, 0x0800, false
   // 0xF800, 0x0600, false
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

extern bool PSRAM_AVAILABLE;
#include "psram_spi.h"
#include "ram_page.h"

uint16_t umb_allocate(uint16_t* psz, uint16_t* err) {
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(!p->allocated && p->sz >= *psz) {
            p->allocated = true;
            update_segment_map(
                p->seg,
                PSRAM_AVAILABLE ? write8psram  : ram_page_write  ,
                PSRAM_AVAILABLE ? write16psram : ram_page_write16,
                PSRAM_AVAILABLE ? read8psram   : ram_page_read   ,
                PSRAM_AVAILABLE ? read16psram  : ram_page_read16
            );
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
