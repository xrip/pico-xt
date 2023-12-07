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
        }
    }
}

bool umb_in_use(uint32_t addr32) {
    uint16_t paragraph = addr32 >> 4;
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(p->allocated && p->seg <= paragraph && p->seg + p->sz > paragraph)
            return true;
    }
    return false;
}

uint16_t umb_allocate(uint16_t* psz, uint16_t* err) {
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(!p->allocated && p->sz >= *psz) {
            p->allocated = true;
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
            *err = XMS_SUCCESS_CODE;
            return 0x0000;
        }
    }
    *err = XMS_ERROR_CODE;
    return 0x00B2; // invalid seg
}
#endif
