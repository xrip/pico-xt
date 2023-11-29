#include "a20.h"
#include <string.h>
#include <stdio.h>

bool extra_mem_initialized = true; // TODO: reinit failed
static uint16_t a20_enable_count = 0;

void set_a20_global_enabled() {
    a20_enable_count++;
    //char tmp[40]; sprintf(tmp, "A20: GSETu %d", a20_enable_count); logMsg(tmp);
}
void set_a20_global_diabled() {
    if (a20_enable_count)
        a20_enable_count--;
    //char tmp[40]; sprintf(tmp, "A20: GSETd %d", a20_enable_count); logMsg(tmp);
}

bool get_a20_enabled() {
    //char tmp[40]; sprintf(tmp, "A20: GET %d", a20_enable_count); logMsg(tmp);
    return a20_enable_count > 0;
}

void set_a20_enabled(bool v) {
    //char tmp[40]; sprintf(tmp, "A20: SET %s", v ? "ON" : "OFF"); logMsg(tmp);
    if (a20_enable_count == 1 && !v) a20_enable_count--;
    if (v && !a20_enable_count) ++a20_enable_count;
}

// Maximum number of map entries in the e820 map
#define BUILD_MAX_E820 32
int e820_count = 0;
struct e820entry e820_list[BUILD_MAX_E820];

// Remove an entry from the e820_list.
static void remove_e820(int i) {
    e820_count--;
    memmove(&e820_list[i], &e820_list[i+1], sizeof(e820_list[0]) * (e820_count - i));
}

// Insert an entry in the e820_list at the given position.
static void insert_e820(int i, uint64_t start, uint64_t size, uint32_t type) {
    if (e820_count >= BUILD_MAX_E820) {
        //warn_noalloc();
        return;
    }
    memmove(&e820_list[i+1], &e820_list[i], sizeof(e820_list[0]) * (e820_count - i));
    e820_count++;
    struct e820entry *e = &e820_list[i];
    e->start = start;
    e->size = size;
    e->type = type;
}

static const char* e820_type_name(uint32_t type) {
    switch (type) {
    case E820_RAM:      return "RAM";
    case E820_RESERVED: return "RESERVED";
    case E820_ACPI:     return "ACPI";
    case E820_NVS:      return "NVS";
    case E820_UNUSABLE: return "UNUSABLE";
    default:            return "UNKNOWN";
    }
}

// Show the current e820_list.
static void dump_map(void) {
    printf( "e820 map has %d items:\n", e820_count);
    for (int i = 0; i < e820_count; i++) {
        struct e820entry *e = &e820_list[i];
        uint64_t e_end = e->start + e->size;
        printf( "  %d: %016llx - %016llx = %d %s\n", i, e->start, e_end, e->type, e820_type_name(e->type));
    }
}

#define E820_HOLE ((uint32_t)-1) // Used internally to remove entries

// Add a new entry to the list.  This scans for overlaps and keeps the
// list sorted.
void e820_add(uint64_t start, uint64_t size, uint32_t type) {
    printf( "Add to e820 map: %08llx %08llx %d\n", start, size, type);
    if (! size)
        // Huh?  Nothing to do.
        return;
    // Find position of new item (splitting existing item if needed).
    uint64_t end = start + size;
    int i;
    for (i = 0; i < e820_count; i++) {
        struct e820entry *e = &e820_list[i];
        uint64_t e_end = e->start + e->size;
        if (start > e_end)
            continue;
        // Found position - check if an existing item needs to be split.
        if (start > e->start) {
            if (type == e->type) {
                // Same type - merge them.
                size += start - e->start;
                start = e->start;
            } else {
                // Split existing item.
                e->size = start - e->start;
                i++;
                if (e_end > end)
                    insert_e820(i, end, e_end - end, e->type);
            }
        }
        break;
    }
    // Remove/adjust existing items that are overlapping.
    while (i < e820_count) {
        struct e820entry *e = &e820_list[i];
        if (end < e->start)
            // No overlap - done.
            break;
        uint64_t e_end = e->start + e->size;
        if (end >= e_end) {
            // Existing item completely overlapped - remove it.
            remove_e820(i);
            continue;
        }
        // Not completely overlapped - adjust its start.
        e->start = end;
        e->size = e_end - end;
        if (type == e->type) {
            // Same type - merge them.
            size += e->size;
            remove_e820(i);
        }
        break;
    }
    // Insert new item.
    if (type != E820_HOLE)
        insert_e820(i, start, size, type);
    //dump_map();
}

// Remove any definitions in a memory range (make a memory hole).
void e820_remove(uint64_t start, uint64_t size) {
    e820_add(start, size, E820_HOLE);
}

// Report on final memory locations.
void e820_prepboot(void) {
    dump_map();
}

// from cpu.c
void writew86(uint32_t addr32, uint16_t value);
void write86(uint32_t addr32, uint8_t value);
uint16_t readw86(uint32_t addr32);
uint8_t read86(uint32_t addr32);
uint8_t set_a20(bool i) {

}
/*
void i15_87h(uint16_t words_to_move, uint32_t gdt_far) {
    bool prev_a20_enable = is_a20_enabled; // enable A20 line if not
    is_a20_enabled = true;
    uint16_t source_segment_szb = readw86(gdt_far + 0x10); // (2*CX-1) or grater
    uint32_t linear_source_addr24 = read86(gdt_far + 0x14);; // 24 bit addrss of source
    linear_source_addr24 = (linear_source_addr24 << 8) + read86(gdt_far + 0x13);
    linear_source_addr24 = (linear_source_addr24 << 8) + read86(gdt_far + 0x12);
    uint16_t dest_segment_szb = readw86(gdt_far + 0x18); // (2*CX-1) or grater
    uint32_t linear_dest_addr24 = read86(gdt_far + 0x1C); // 24 bit addrss of source
    linear_dest_addr24 = (linear_dest_addr24 << 8) + readw86(gdt_far + 0x1B);
    linear_dest_addr24 = (linear_dest_addr24 << 8) + readw86(gdt_far + 0x1A);
    char tmp[80]; sprintf(tmp, "INT15h FN 87h words_to_move: %d src: %Xh (%d) dst: %Xh (%d)",
                                words_to_move,
                                linear_source_addr24, source_segment_szb,
                                linear_dest_addr24, dest_segment_szb); logMsg(tmp);
    for (int offset = 0; offset < (words_to_move << 1); offset += 2) {
        // TODO: block move by memory manager
        uint16_t d = readw86(linear_source_addr24 + offset);
        writew86(linear_dest_addr24 + offset, d);
    }
    is_a20_enabled = prev_a20_enable; // restore prev. A20 line state    
}
*/
typedef struct xmm_handler {
    uint16_t seg;
    uint16_t sz_kb;
    uint8_t locks_cnt;
} xmm_handler_t;

#define MAX_XMM_HANDLERS 32

static xmm_handler_t xmm_handlers[MAX_XMM_HANDLERS] = { 0 };

__always_inline uint8_t xmm_free_handlers() {
    uint8_t res = 0;
    for (uint16_t i = 0; i < MAX_XMM_HANDLERS; ++i) {
        if (!xmm_handlers[i].sz_kb) res++;
    }
    return res;
}

uint8_t /*BL*/ move_ext_mem_block(uint32_t tbl_addr) {
    uint32_t w0 = readw86(tbl_addr++); tbl_addr++;
    uint32_t w1 = readw86(tbl_addr++); tbl_addr++;
    uint32_t len = (w1 << 16) | w0; // bytes to transfer
    uint16_t s_h = readw86(tbl_addr++); tbl_addr++; // handler of source
    if (s_h > MAX_XMM_HANDLERS) return 0xA3;
    uint32_t s0 = readw86(tbl_addr++); tbl_addr++;
    uint32_t s1 = readw86(tbl_addr++); tbl_addr++;
    uint32_t s_o = s_h == 0 ?
        ((s1 << 4) + s0) :
        ((s1 << 16) | s0) + (xmm_handlers[s_h - 1].seg << 4); // source offset
    uint16_t d_h = readw86(tbl_addr++); tbl_addr++; // handler of destination
    if (d_h > MAX_XMM_HANDLERS) return 0xA3;
    uint32_t d0 = readw86(tbl_addr++); tbl_addr++;
    uint32_t d1 = readw86(tbl_addr);
    uint32_t d_o = d_h == 0 ?
        ((d1 << 4) + d0) :
        ((d1 << 16) | d0) +  (xmm_handlers[d_h - 1].seg << 4); // destination offset
    for (uint32_t s_i = 0; s_o < len; s_o += 2, d_o += 2) { // TODO: block move
        uint16_t d = readw86(s_o);
        writew86(d_o, d);
    }
    return 0;
}

uint8_t /*BL*/ lock_ext_mem_block(uint16_t handler) {
    // TODO: error handling
    xmm_handlers[handler - 1].locks_cnt++;
    return 0;
}

uint8_t /*BL*/ unlock_ext_mem_block(uint16_t handler) {
    // TODO: error handling
    xmm_handlers[handler - 1].locks_cnt--;
    return 0;
}

__always_inline uint16_t xmm_used_kb() {
    uint16_t res = 0;
    for (uint16_t i = 0; i < MAX_XMM_HANDLERS; ++i) {
        res += xmm_handlers[i].sz_kb;
    }
    return res;
}

__always_inline uint16_t allocate_xmm_page(uint16_t kbs, uint8_t* pBL) {
    uint16_t free_kb = TOTAL_XMM_KB - 64 - xmm_used_kb();
    if (free_kb < kbs) {
        *pBL = 0xA0; // not enough memory
        return 0;
    }
    uint16_t seg = (TOTAL_XMM_KB - 64) << 6;
    for (uint16_t i = 0; i < MAX_XMM_HANDLERS; ++i) {
        xmm_handler_t *ph = &xmm_handlers[i];
        if (ph->sz_kb) {
            seg += ph->sz_kb << 6;
            continue;
        }
        uint16_t seg_candidate = seg;
        uint16_t next_seg = seg_candidate + (kbs << 6);
        for (uint16_t j = j + 1; j < MAX_XMM_HANDLERS; ++j) {
            xmm_handler_t *phj = &xmm_handlers[j];
            if (phj->sz_kb && phj->seg < seg_candidate) {
                seg_candidate = 0;
                break;
            }
        }
        if (seg_candidate != 0) {
            ph->seg = seg_candidate;
            ph->sz_kb = kbs;
            *pBL = 0;
            return i + 1;
        }
    }
    *pBL = 0xA1; // all available handlers are allocated
    return 0;
}

typedef struct umb {
    uint16_t seg;
    uint16_t sz; // paragraphs
    bool allocated;
} umb_t;

#define UMB_BLOCKS 8

static umb_t umb_blocks[UMB_BLOCKS] = {
    0xA000, 0x0800, false, // TODO: remove on EGA enabled
    0xA800, 0x0100, false, // TODO: remove on EGA enabled
    0xB000, 0x0800, false, // TODO: remove on EGA enabled
    0xC800, 0x0800, false,
    0xE000, 0x0800, false,
    0xE800, 0x0800, false,
    0xF000, 0x0800, false,
    0xF800, 0x0600, false
};

bool umb_in_use(uint32_t addr32) {
    uint16_t paragraph = addr32 >> 4;
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(p->allocated && p->seg <= paragraph && p->seg + p->sz > paragraph)
            return true;
    }
    return false;
}

#define XMS_ERROR_CODE   0x0000
#define XMS_SUCCESS_CODE 0x0001

static uint16_t umb_allocate(uint16_t* psz, uint16_t* err) {
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

static uint16_t umb_deallocate(uint16_t* seg, uint16_t* err) {
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

bool hma_in_use = false;

void if_reboot_detected() {
    if (!extra_mem_initialized) {
        extra_mem_initialized = true;
        // logMsg("REBOOT WAS DETECTED");
        sleep_ms(2000);
        emm_reboot();
        xmm_reboot();
    }
}

uint8_t xms_fn() {
    char tmp[80];
    if_reboot_detected();
    switch(CPU_AH) {
        case 0x00: // XMS 00H: Get XMS Version Number
            sprintf(tmp, "XMS FN %02Xh: XMS Sec ver 2.0; Drv ver 1.0; HMA available", CPU_AH);
            CPU_AX = 0x0200; // spec. version
            CPU_BX = 0x0100; // driver version
            CPU_DX = 0x0001; // HMA installed
            break;
        case 0x01: // XMS 01H: Request High Memory Area
            if (hma_in_use) {
                sprintf(tmp, "XMS FN %02Xh: HMA requested to allocate %04Xh bytes (rejected - in use)", CPU_AH, CPU_DX);
                CPU_AX = XMS_ERROR_CODE; // ERROR
                CPU_BL = 0x91; // HMA is already in use
            } else if (get_a20_enabled()) {
                sprintf(tmp, "XMS FN %02Xh: HMA requested to allocate %04Xh bytes (rejected - A20 is off)", CPU_AH, CPU_DX);
                CPU_AX = XMS_ERROR_CODE; // ERROR
                CPU_BL = 0x82; // A20 is OFF
            } else {
                sprintf(tmp, "XMS FN %02Xh: HMA requested to allocate %04Xh bytes (allocated)", CPU_AH, CPU_DX);
                hma_in_use = true;
                CPU_AX = XMS_SUCCESS_CODE; // successful
                CPU_BL = 0x00;
            }
            break;
        case 0x02: // XMS 02H: Release High Memory Area
            hma_in_use = false;
            sprintf(tmp, "XMS FN %02Xh: HMA requested to release", CPU_AH);
            CPU_AX = XMS_SUCCESS_CODE; // successful
            CPU_BL = 0x00;
            break;
        case 0x03: // XMS 03H: Global Enable A20
            set_a20_global_enabled();
            sprintf(tmp, "XMS FN %02Xh: Global Enable A20", CPU_AH);
            CPU_AX = get_a20_enabled() ? 0x0001 : 0x0000;
            CPU_BL = 0x00;
            break;
        case 0x05: // XMS 05H: Local Enable A20
            set_a20_enabled(true);
            sprintf(tmp, "XMS FN %02Xh: Local Enable A20", CPU_AH);
            CPU_AX = get_a20_enabled() ? 0x0001 : 0x0000;
            CPU_BL = 0x00;
            break;
        case 0x04: // XMS 04H: Global Disable A20
            set_a20_global_diabled();
            sprintf(tmp, "XMS FN %02Xh: Global Disable A20", CPU_AH);
            CPU_AX = get_a20_enabled() ? 0x0001 : 0x0000;
            CPU_BL = 0x00;
            break;
        case 0x06: // XMS 06H: Local Disable A20
            set_a20_enabled(false);
            sprintf(tmp, "XMS FN %02Xh: Local Disable A20", CPU_AH);
            CPU_AX = get_a20_enabled() ? 0x0001 : 0x0000;
            CPU_BL = 0x00;
            break;
        case 0x07: // XMS 07H: Query A20 State
            CPU_AX = get_a20_enabled() ? 0x0001 : 0x0000;
            sprintf(tmp, "XMS FN 07h: A20 status: %s", CPU_AX ? "ON" : "OFF");
            CPU_BL = 0x00;
            break;
        case 0x08: { // XMS 08H: Query Free Extended Memory
            uint16_t used_kb = xmm_used_kb();
            CPU_AX = TOTAL_XMM_KB - 64 - used_kb; // free XMS - total minus HMA, minus used
            CPU_BL = used_kb + 64 >= TOTAL_XMM_KB ? 0xA0 : 0;
            CPU_DX = TOTAL_XMM_KB - 64; // total XMS amount
            sprintf(tmp, "XMS FN 08h: Free Extended Memory: %dKB of %dKB", CPU_AX, CPU_DX);
            break;
        }
        case 0x09: { // XMS 09H: Allocate Extended Memory Block
                     // DX    desired size of block, in K-bytes
            uint16_t t = CPU_DX;
            // res: DX    XMS handle
            CPU_DX = allocate_xmm_page(CPU_DX, &CPU_BL);
            if (CPU_BL >= 0x80) {
                sprintf(tmp, "XMS FN 09h: Allocate Extended Memory Block: %dKB (rejected)", t);
                CPU_AX = XMS_ERROR_CODE;
                break;
            }
            CPU_AX = XMS_SUCCESS_CODE;
            sprintf(tmp, "XMS FN 09h: Allocate Extended Memory Block: %dKB (allocated #%d)", t, CPU_DX);
            break;
        }
        case 0x0A: { // XMS 0AH: Free Extended Memory Block
            // DX    XMS handle (as obtained via XMS 09H)
            if (CPU_DX > MAX_XMM_HANDLERS) {
                CPU_AX = XMS_ERROR_CODE;
                CPU_BL = 0xA2; // Invalid handler
            } else {
                xmm_handlers[CPU_DX - 1].sz_kb = 0;
                CPU_AX = XMS_SUCCESS_CODE;
                CPU_BL = 0;
            }
            sprintf(tmp, "XMS FN 0Ah: Free Extended Memory Block #%d; res: %02X", CPU_DX, CPU_BL);
            break;
        }
        case 0x0B:
            CPU_BL = move_ext_mem_block(((uint32_t)CPU_DS << 5) + CPU_SI);
            sprintf(tmp, "XMS FN 0Bh: Move Extended Memory Block; BL: %0Xh", CPU_BL);
            CPU_AX = CPU_BL >= 0x80 ? XMS_ERROR_CODE : XMS_SUCCESS_CODE;
            break;
        case 0x0C:
            CPU_BL = lock_ext_mem_block(CPU_DX);
            sprintf(tmp, "XMS FN 0Ch: Lock Extended Memory Block #%0Xh", CPU_DX);
            CPU_AX = CPU_BL >= 0x80 ? XMS_ERROR_CODE : XMS_SUCCESS_CODE;
            break;
        case 0x0D:
            CPU_BL = unlock_ext_mem_block(CPU_DX);
            sprintf(tmp, "XMS FN 0Dh: Unlock Extended Memory Block #%0Xh", CPU_DX);
            CPU_AX = CPU_BL >= 0x80 ? XMS_ERROR_CODE : XMS_SUCCESS_CODE;
            break;
        case 0x0E: { // XMS 0eH: Get Handle Information
            // DX    XMS handle (as obtained via XMS 09H)
            uint16_t handle = CPU_DX;
            // out:
            // BH    current lock count
            // BL    current number of free XMS handles
            // DX    size of the block, in K-bytes
            if (handle > MAX_XMM_HANDLERS) {
                CPU_AX = XMS_ERROR_CODE;
                CPU_BL = 0xA2; // invalid handler
                sprintf(tmp, "XMS FN 0Eh: Handle Information #%d failed (no such id)", handle);
                break;
            }
            CPU_AX = XMS_SUCCESS_CODE; // TODO:
            CPU_BH = xmm_handlers[CPU_DX - 1].locks_cnt;
            CPU_BL = xmm_free_handlers();
            CPU_DX = xmm_handlers[CPU_DX - 1].sz_kb;
            sprintf(tmp, "XMS FN 0Eh: Handle Information #%d allocated %dKB", handle, CPU_DX);
            break;
        }
        case 0x0F: // XMS 0fH: Resize Extended Memory Block
            // BX    desired new size, in K-bytes
            // DX    XMS handle (as obtained via XMS 09H) must be unlocked
            xmm_handlers[CPU_DX - 1].sz_kb = CPU_BX;
            CPU_AX = XMS_SUCCESS_CODE; // TODO: error handling
            CPU_BL = 0;
            sprintf(tmp, "XMS FN 0Fh: Resize Extended Memory Block #%d to %dKB", CPU_DX, CPU_BX);
            break;
        case 0x10: // XMS 10H: Request Upper Memory Block
                   // DX    desired size of UMB, in paragraphs (16-byte units)
            CPU_BX = umb_allocate(&CPU_DX, &CPU_AX);
            sprintf(tmp, "XMS FN 10h: UMB allocation: BX(seg/err): %04Xh; DX(sz): %04Xh; AX(err): %04Xh", CPU_BX, CPU_DX, CPU_AX);
            break;
        case 0x11: // XMS 10H: Release Upper Memory Block
                   // DX    desired size of UMB, in paragraphs (16-byte units)
            CPU_BX = umb_deallocate(&CPU_DX, &CPU_AX);
            sprintf(tmp, "XMS FN 11h: UMB dellocation: BX(seg/err): %04Xh; DX(sz): %04Xh; AX(err): %04Xh", CPU_BX, CPU_DX, CPU_AX);
            break;
        default:
            sprintf(tmp, "XMS FN %2Xh: ERROR (not implemented)", CPU_AH);
            CPU_AX = XMS_ERROR_CODE; // ERROR
            CPU_BL = 0x80; // Function not implemented
    }
    logMsg(tmp);
    return 0xCB /* CB RETF */;
}

void xmm_reboot() {
    for (int i = 0; i < UMB_BLOCKS; ++i) {
        umb_t *p = &umb_blocks[i];
        if(p->allocated) {
            p->allocated = false;
        }
    }
    hma_in_use = false;
    a20_enable_count = 0;
    memset(&xmm_handlers, 0, sizeof xmm_handlers);
}
