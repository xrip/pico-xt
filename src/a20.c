#include "a20.h"
#include <string.h>
#include <stdio.h>

#ifdef XMS_DRIVER

uint8_t hma_in_use_count = 0; // W/A DOS try to use it twice
static bool xms_in_use = false; // XMS 3.0 requires to hook INT 15h only after first non-version XMS call

static char tmp[80];

void i15_87h(uint16_t words_to_move, uint32_t gdt_far) {
    uint16_t source_segment_szb = readw86(gdt_far + 0x10); // (2*CX-1) or grater
    uint32_t linear_source_addr24 = read86(gdt_far + 0x14);; // 24 bit addrss of source
    linear_source_addr24 = (linear_source_addr24 << 8) + read86(gdt_far + 0x13);
    linear_source_addr24 = (linear_source_addr24 << 8) + read86(gdt_far + 0x12);
    uint16_t dest_segment_szb = readw86(gdt_far + 0x18); // (2*CX-1) or grater
    uint32_t linear_dest_addr24 = read86(gdt_far + 0x1C); // 24 bit addrss of source
    linear_dest_addr24 = (linear_dest_addr24 << 8) + readw86(gdt_far + 0x1B);
    linear_dest_addr24 = (linear_dest_addr24 << 8) + readw86(gdt_far + 0x1A);
    sprintf(tmp, "INT15h FN 87h words_to_move: %d src: %Xh (%d) dst: %Xh (%d)",
                                words_to_move,
                                linear_source_addr24, source_segment_szb,
                                linear_dest_addr24, dest_segment_szb); logMsg(tmp);
    for (int offset = 0; offset < (words_to_move << 1); offset += 2) {
        // TODO: block move by memory manager
        uint16_t d = readw86(linear_source_addr24 + offset);
        writew86(linear_dest_addr24 + offset, d);
    }
}

typedef struct xmm_handle {
    uint16_t handle;
    uint16_t sz_kb;
    uint8_t locks_cnt;
} xmm_handle_t;

#if XMS_OVER_HMA_KB
#define MAX_XMM_HANDLES 15 // TODO: 60 ??
static xmm_handle_t xmm_handles[MAX_XMM_HANDLES] = { 0 };
#endif

INLINE uint8_t xmm_free_handles() {
    uint8_t res = 0;
#if XMS_OVER_HMA_KB
    for (uint16_t i = 0; i < MAX_XMM_HANDLES; ++i) {
        if (!xmm_handles[i].sz_kb) res++;
    }
#endif
    return res;
}

INLINE uint16_t xmm_handle_size(uint16_t h) {
#if XMS_OVER_HMA_KB
    if (h == 0) {
        return 0;
    }
    uint16_t res = 0;
    for (uint16_t i = 0; i < MAX_XMM_HANDLES; ++i) {
        if (xmm_handles[i].handle == h) {
            res += XMS_STATIC_PAGE_KBS;
        }
    }
    return res;
#else
    return 0;
#endif
}

INLINE uint16_t handle2seg(uint16_t handle) {
    return (handle - 1) * XMS_STATIC_PAGE_PHARAGRAPS + BASE_XMS_HANLES_SEG;
}

INLINE uint8_t /*BL*/ move_ext_mem_block(uint32_t tbl_addr) {
#if XMS_OVER_HMA_KB
    uint32_t w0 = readw86(tbl_addr++); tbl_addr++;
    uint32_t w1 = readw86(tbl_addr++); tbl_addr++;
    uint32_t len = (w1 << 16) | w0; // bytes to transfer
    sprintf(tmp, "XMS FN 0Bh LEN:%08Xh = %d (%dK)", len, len, len >> 10); logMsg(tmp);
    
    uint16_t s_h = readw86(tbl_addr++); tbl_addr++; // handle of source
    if (s_h > MAX_XMM_HANDLES) {
        return 0xA3;
    }
    if (s_h > 0 && xmm_handles[s_h - 1].locks_cnt) {
        return 0xAB; // handle is locked
    }
    uint32_t s0 = readw86(tbl_addr++); tbl_addr++;
    uint32_t s1 = readw86(tbl_addr++); tbl_addr++;
    uint32_t shs = s_h == 0 ? 0 : handle2seg(s_h);
    uint32_t s_o = s_h == 0 ?
        ((s1 << 4) + s0) :
        ((s1 << 16) | s0) + (shs << 4); // source offset
    if (s_h == 0) { sprintf(tmp, "XMS FN 0Bh Src addr32:%08Xh [%04X:%04X]", s_o, s1, s0); logMsg(tmp); }
    else { sprintf(tmp, "XMS FN 0Bh Src addr32:%08Xh (%d)", s_o, s_h); logMsg(tmp); }
    
    uint16_t d_h = readw86(tbl_addr++); tbl_addr++; // handle of destination
    if (d_h > MAX_XMM_HANDLES) {
        return 0xA3;
    }
    if (d_h > 0 && xmm_handles[d_h - 1].locks_cnt) {
        return 0xAB; // handle is locked
    }
    uint32_t d0 = readw86(tbl_addr++); tbl_addr++;
    uint32_t d1 = readw86(tbl_addr);
    uint32_t dhs = d_h == 0 ? 0 : handle2seg(d_h);
    uint32_t d_o = d_h == 0 ?
        ((d1 << 4) + d0) :
        ((d1 << 16) | d0) + (dhs << 4); // destination offset
    if (d_h == 0) { sprintf(tmp, "XMS FN 0Bh Dst addr32:%08Xh [%04X:%04X]", d_o, d1, d0); logMsg(tmp); }
    else { sprintf(tmp, "XMS FN 0Bh Dst addr32:%08Xh (%d)", d_o, d_h); logMsg(tmp); }

    for (uint32_t s_i = 0; s_o < len; s_o += 2, d_o += 2) { // TODO: block move
        uint16_t d = readw86(s_o);
        writew86(d_o, d);
    }
    return 0;
#else
    return 0x86;
#endif
}

INLINE uint8_t /*BL*/ lock_ext_mem_block(uint16_t handle, uint16_t* pw1, uint16_t* pw0) {
#if XMS_OVER_HMA_KB
    if(handle > MAX_XMM_HANDLES) {
        return 0xA2; // the handle is invalid
    }
    for (uint16_t i = handle; i < MAX_XMM_HANDLES; ++i) {
        xmm_handle_t* p =  &xmm_handles[i];
        if (p->handle == handle)
            xmm_handles[i].locks_cnt++;
    }
    uint32_t addr32 = ((uint32_t)handle2seg(handle)) << 4;
    *pw1 = addr32 >> 16;
    *pw0 = addr32;
    return 0;
#else
    return 0x86;
#endif
}

uint8_t /*BL*/ unlock_ext_mem_block(uint16_t handle) {
#if XMS_OVER_HMA_KB
    if(handle > MAX_XMM_HANDLES) {
        return 0xA2; // the handler is invalid
    }
    for (uint16_t i = handle; i < MAX_XMM_HANDLES; ++i) {
        xmm_handle_t* p =  &xmm_handles[i];
        if (p->handle == handle) {
            if(p->locks_cnt == 0) {
                return 0xAA; // is not proper locked
            }
            p->locks_cnt--;
        }
    }
    return 0;
#else
    return 0x86;
#endif
}

INLINE uint16_t xmm_used_kb() {
    uint16_t res = 0;
#if XMS_OVER_HMA_KB
    for (uint16_t i = 0; i < MAX_XMM_HANDLES; ++i) {
        if (xmm_handles[i].handle)
            res += XMS_STATIC_PAGE_KBS;
    }
#endif
    return res;
}

INLINE uint16_t get_handle_feets(uint16_t kbs) {
#if XMS_OVER_HMA_KB
    for (uint16_t i = 0; i < MAX_XMM_HANDLES; ++i) {
        xmm_handle_t *ph = &xmm_handles[i];
        sprintf(tmp, "get_handle_feets(%d) H:%d SZ:%d LC:%d", kbs, ph->handle, ph->sz_kb, ph->locks_cnt); logMsg(tmp);
        if (ph->handle) { // allocated
            continue;
        }
        uint16_t i_cand = i;
        uint16_t cont_blocK = 0;
        while (!ph->handle) {
            cont_blocK += XMS_STATIC_PAGE_KBS;
            if (cont_blocK >= kbs) {
                return i_cand;
            }
            ph = &xmm_handles[++i];
        }
    }
#endif
    return 0xFFFF;
}

INLINE uint16_t xmm_free_kb() {
    uint16_t res = 0;
#if XMS_OVER_HMA_KB
    for (uint16_t i = 0; i < MAX_XMM_HANDLES; ++i) {
        if (!xmm_handles[i].handle)
            res += XMS_STATIC_PAGE_KBS;
    }
#endif
    return res;
}

INLINE uint16_t xmm_max_block_kb() {
    uint16_t max = 0;
#if XMS_OVER_HMA_KB
    uint16_t ri = 0;
    for (uint16_t i = 0; i < MAX_XMM_HANDLES; ++i) {
        if (xmm_handles[i].handle) {
            if (ri > max) {
                max = ri;
            }
            ri = 0;
        } else {
            ri += XMS_STATIC_PAGE_KBS;
        }
    }
    if (ri > max) {
        max = ri;
    }
#endif
    return max;
}

INLINE uint16_t allocate_xmm_page(uint16_t kbs, uint8_t* pBL) {
    uint16_t i = get_handle_feets(kbs);
    if (i == 0xFFFF) {
        *pBL = 0xA0; // not enough memory
        return 0;
    }
#if XMS_OVER_HMA_KB
    uint16_t accumulated = 0;
    uint16_t h = i + 1;
    for (; i < MAX_XMM_HANDLES; ++i) {
        accumulated += XMS_STATIC_PAGE_KBS;
        xmm_handle_t *ph = &xmm_handles[i];
        ph->sz_kb = accumulated > kbs ? accumulated - kbs : XMS_STATIC_PAGE_KBS;
        ph->locks_cnt = 0;
        ph->handle = h;
        if (accumulated > kbs) {
            *pBL = 0;
            return h;
        }
    }
#endif
    *pBL = 0xA1; // all available handlers are allocated
    return 0;
}

INLINE uint8_t resize_xmm_page(uint16_t handle, uint16_t kbs) {
#if XMS_OVER_HMA_KB
    uint8_t save_lock = 0;
    for (uint16_t i = handle - 1; i < MAX_XMM_HANDLES; ++i) {
        xmm_handle_t* p =  &xmm_handles[i];
        if (p->handle != handle) {
            break;
        }
        p->handle = 0;
        save_lock = p->locks_cnt;
        p->locks_cnt = 0;
        p->sz_kb = 0;
    }
    // TODO: corner cases
    uint16_t accumulated = 0;
    for (uint16_t i = handle - 1; i < MAX_XMM_HANDLES; ++i) {
        accumulated += XMS_STATIC_PAGE_KBS;
        xmm_handle_t *ph = &xmm_handles[i];
        ph->sz_kb = accumulated > kbs ? accumulated - kbs : XMS_STATIC_PAGE_KBS;
        ph->locks_cnt = save_lock;
        ph->handle = handle;
        if (accumulated > kbs) {
            return 0;
        }
    }
#endif
    return 0xA1; // all available handlers are allocated
}

INLINE uint8_t deallocate_xmm_page(uint16_t h) {
#if XMS_OVER_HMA_KB
    if (CPU_DX > MAX_XMM_HANDLES) {
        return 0xA2; // Invalid handler
    }
    for (uint16_t i = h - 1; i < MAX_XMM_HANDLES; ++i) {
        xmm_handle_t* p =  &xmm_handles[i];
        if (p->handle != h) {
            break;
        }
        if (p->locks_cnt) {
            return 0xAB; // the handle is locked
        }
        p->handle = 0;
        p->sz_kb = 0;
    }
    return 0;
#else
    return 0x86;
#endif
}

bool INT_2Fh() {
    switch (CPU_AX) {
        case 0x4300: {
            logMsg("HIMEM.SYS (XMM) detection passed");
            CPU_AL = 0x80;
            return true;
        }
        case 0x4310: {
            logMsg("HIMEM.SYS (XMM) Entry Address: 0000:03FF"); // W/A
            CPU_ES = XMS_FN_CS; // 
            CPU_BX = XMS_FN_IP; // 
            return true;
        }
    }
    return false;
}

bool INT_15h() {
    switch (CPU_AH) {
        case 0xDA:
            if (CPU_AL == 0x88) {
                CPU_CL = 0;
                CPU_BX = ON_BOARD_RAM_KB - 1024; // - (hma_in_use ? 64 : 0);
                cf = 0;
                logMsg("INT15! DA88h mem info");
                return true;
            }
            return false;
        case 0x8A: //  количество блоков по 1Кб свыше 64Мб
            CPU_DX = 0; //  старшая часть размера памяти
            CPU_AX = 0; // ON_BOARD_RAM_KB - 1024; // - (hma_in_use ? 64 : 0);
            cf = 0;
            logMsg("INT15! 8Ah mem info");
            return true;
        case 0xC7:
            // DS:SI to memory map
            // TODO:
            logMsg("INT15! C7h mem-map info");
            return false;
        case 0x24:
            switch (CPU_AL) {
                case 0x00:
                    notify_a20_line_state_changed(true);
                    cf = 0;
                    CPU_AH = 0;
                    logMsg("INT15! 2400 turn on A20_ENABLE_BIT");
                    return true;
                case 0x01:
                    notify_a20_line_state_changed(false);
                    cf = 0;
                    CPU_AH = 0;
                    logMsg("INT15! 2401 turn off A20_ENABLE_BIT");
                    return true;
                case 0x02:
                    CPU_AL = is_a20_line_open();
                    cf = 0;
                    CPU_AH = 0;
                    sprintf(tmp, "INT15! 2402 AL: 0x%X (A20 line)", CPU_AL); logMsg(tmp);
                    return true;
                case 0x03:
                    CPU_BX = 0b11;
                    CPU_AH = 0;
                    cf = 0;
                    sprintf(tmp, "INT15! 2403 BX: %xh", CPU_BX); logMsg(tmp);
                    return true;
            }
            break;
        case 0x87:
            if (!xms_in_use) {
                return false;
            }
            else { // Memory block move EMS
                uint16_t words_to_move = CPU_CX;
                uint32_t gdt_far = (CPU_ES << 4) + CPU_SI;
                i15_87h(words_to_move, gdt_far);
            }
            CPU_AH = 0;
            cf = 0;
            return true;
        case 0x88: // memory info
#if ON_BOARD_RAM_KB > 64 * 1024
            CPU_AX = 63 * 1024;
#else
            CPU_AX = ON_BOARD_RAM_KB - 1024;
#endif
            cf = 0;
            return true;
        case 0xE8:
            switch (CPU_AL) {
                case 0x01:
#if ON_BOARD_RAM_KB > 16*1024
                    CPU_CX = 1024 * 15; // 15MB
                    CPU_DX = (uint16_t)(ON_BOARD_RAM_KB - 16 * 1024) / 64;
#else
                    CPU_CX = ON_BOARD_RAM_KB - 1024; // - (hma_in_use ? 64 : 0);
                    CPU_DX = 0;
#endif
                    CPU_AX = CPU_CX;
                    CPU_BX = CPU_DX;
                    cf = 0;
                    return true;
                case 0x20: // TODO: ES:DI - destination for the table
                    return false;
            }
            return false;
        default: {
            // sprintf(tmp, "INT 15h CPU_AH: 0x%X; CPU_AL: 0x%X", CPU_AH, CPU_AL); logMsg(tmp);
        }
    }
    return false;
}

uint8_t xms_fn() {
    char tmp[80];
    if (CPU_AH == 0x00) { // XMS 00H: Get XMS Version Number
#ifdef XMS_HMA
        sprintf(tmp, "XMS FN %02Xh: XMS Sec ver 3.0; Drv ver 1.01; HMA available", CPU_AH);
        CPU_DX = 0x0001; // HMA installed
#else
        sprintf(tmp, "XMS FN %02Xh: XMS Sec ver 3.0; Drv ver 1.01; HMA is turned off", CPU_AH);
        CPU_DX = 0x0000; // HMA not installed
#endif
        CPU_AX = 0x0300; // spec. version
        CPU_BX = 0x0101; // driver version
    } else {
        xms_in_use = true;
        switch(CPU_AH) {
#ifdef XMS_HMA
        case 0x01: // XMS 01H: Request High Memory Area
            if (hma_in_use_count > 1) {
                sprintf(tmp, "XMS FN %02Xh: HMA requested to allocate %04Xh bytes (rejected - in use)", CPU_AH, CPU_DX);
                CPU_AX = XMS_ERROR_CODE; // ERROR
                CPU_BL = 0x91; // HMA is already in use
            } else if (!is_a20_line_open()) {
                sprintf(tmp, "XMS FN %02Xh: HMA requested to allocate %04Xh bytes (rejected - A20 is off)", CPU_AH, CPU_DX);
                CPU_AX = XMS_ERROR_CODE; // ERROR
                CPU_BL = 0x82; // A20 is OFF
            } else {
                sprintf(tmp, "XMS FN %02Xh: HMA requested to allocate %04Xh bytes (allocated)", CPU_AH, CPU_DX);
                hma_in_use_count++;
                CPU_AX = XMS_SUCCESS_CODE; // successful
                CPU_BL = 0x00;
            }
            break;
        case 0x02: // XMS 02H: Release High Memory Area
            hma_in_use_count = 0;
            sprintf(tmp, "XMS FN %02Xh: HMA requested to release", CPU_AH);
            CPU_AX = XMS_SUCCESS_CODE; // successful
            CPU_BL = 0x00;
            break;
#endif
        case 0x03: // XMS 03H: Global Enable A20
            notify_a20_line_state_changed(true);
            sprintf(tmp, "XMS FN %02Xh: Global Enable A20", CPU_AH);
            CPU_AX = is_a20_line_open() ? 0x0001 : 0x0000;
            CPU_BL = 0x00;
            break;
        case 0x05: // XMS 05H: Local Enable A20
            notify_a20_line_state_changed(true);
            sprintf(tmp, "XMS FN %02Xh: Local Enable A20", CPU_AH);
            CPU_AX = is_a20_line_open() ? 0x0001 : 0x0000;
            CPU_BL = 0x00;
            break;
        case 0x04: // XMS 04H: Global Disable A20
            notify_a20_line_state_changed(false);
            sprintf(tmp, "XMS FN %02Xh: Global Disable A20", CPU_AH);
            CPU_AX = is_a20_line_open() ? 0x0001 : 0x0000;
            CPU_BL = 0x00;
            break;
        case 0x06: // XMS 06H: Local Disable A20
            notify_a20_line_state_changed(false);
            sprintf(tmp, "XMS FN %02Xh: Local Disable A20", CPU_AH);
            CPU_AX = is_a20_line_open() ? 0x0001 : 0x0000;
            CPU_BL = 0x00;
            break;
        case 0x07: // XMS 07H: Query A20 State
            CPU_AX = is_a20_line_open() ? 0x0001 : 0x0000;
            sprintf(tmp, "XMS FN 07h: Query A20 status: %s", CPU_AX ? "ON" : "OFF");
            CPU_BL = 0x00;
            break;
        case 0x08: { // XMS 08H: Query Free Extended Memory
            uint16_t t = xmm_max_block_kb();
            CPU_AX = t > RESERVED_XMS_KB ? t - RESERVED_XMS_KB : t; // TODO: adjust UMB and HMA usage
            t = xmm_free_kb();
            CPU_DX = t > RESERVED_XMS_KB ? t - RESERVED_XMS_KB : t; // total free XMS amount
            sprintf(tmp, "XMS FN 08h: Size of largest block: %dKB of %dKB free XMS space", CPU_AX, CPU_DX);
            CPU_BL = 0;
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
            CPU_BL = deallocate_xmm_page(CPU_DX);
            CPU_AX = CPU_BL > 0x80 ? XMS_ERROR_CODE : XMS_SUCCESS_CODE;
            sprintf(tmp, "XMS FN 0Ah: Free Extended Memory Block #%d; res: %02X", CPU_DX, CPU_BL);
            break;
        }
        case 0x0B:
            sprintf(tmp, "XMS FN 0Bh: Move Extended Memory Block: TBL %04X:%04X", CPU_DS, CPU_SI); logMsg(tmp);
            CPU_BL = move_ext_mem_block(((uint32_t)CPU_DS << 4) + CPU_SI);
            if (CPU_BL > 0x80) {
                sprintf(tmp, "XMS FN 0Bh: Move Extended Memory Block failed... BL: %02X", CPU_BL);
            }
            CPU_AX = CPU_BL >= 0x80 ? XMS_ERROR_CODE : XMS_SUCCESS_CODE;
            break;
        case 0x0C: {
            uint16_t handler = CPU_DX;
            CPU_BL = lock_ext_mem_block(handler, &CPU_DX, &CPU_BX);
            sprintf(tmp, "XMS FN 0Ch: Lock Extended Memory Block #%d %04X:%044X err: %02X", handler, CPU_DX, CPU_BX, CPU_BL);
            CPU_AX = CPU_BL >= 0x80 ? XMS_ERROR_CODE : XMS_SUCCESS_CODE;
            break;
        }
        case 0x0D:
            CPU_BL = unlock_ext_mem_block(CPU_DX);
            sprintf(tmp, "XMS FN 0Dh: Unlock Extended Memory Block #%d", CPU_DX);
            CPU_AX = CPU_BL >= 0x80 ? XMS_ERROR_CODE : XMS_SUCCESS_CODE;
            break;
        case 0x0E: { // XMS 0eH: Get Handle Information
            // DX    XMS handle (as obtained via XMS 09H)
            uint16_t handle = CPU_DX;
#if XMS_OVER_HMA_KB
            // out:
            // BH    current lock count
            // BL    current number of free XMS handles
            // DX    size of the block, in K-bytes
            if (handle > MAX_XMM_HANDLES) {
                CPU_AX = XMS_ERROR_CODE;
                CPU_BL = 0xA2; // invalid handler
                sprintf(tmp, "XMS FN 0Eh: Handle Information #%d failed (no such id)", handle);
                break;
            }
            CPU_AX = XMS_SUCCESS_CODE; // TODO:
            CPU_BH = xmm_handles[CPU_DX - 1].locks_cnt;
            CPU_BL = xmm_free_handles();
            CPU_DX = xmm_handle_size(CPU_DX);
#else
            CPU_BL = 0x86;
            CPU_AX = XMS_ERROR_CODE;
#endif
            sprintf(tmp, "XMS FN 0Eh: Handle Information #%d allocated %dKB", handle, CPU_DX);
            break;
        }
        case 0x0F: // XMS 0fH: Resize Extended Memory Block
            // BX    desired new size, in K-bytes
            // DX    XMS handle (as obtained via XMS 09H) must be unlocked
#if XMS_OVER_HMA_KB
            if (CPU_DX > MAX_XMM_HANDLES) {
                sprintf(tmp, "XMS FN 0Fh: Resize Extended Memory Block (invalid hndl) #%d to %dKB", CPU_DX, CPU_BX);
                CPU_AX = XMS_ERROR_CODE;
                CPU_BL = 0xA2; // Invalid handler
                break;
            }
            if (CPU_BX > TOTAL_XMM_KB - 64 - (xmm_used_kb() - xmm_handle_size(CPU_DX))) {
                sprintf(tmp, "XMS FN 0Fh: Resize Extended Memory Block (failed) #%d to %dKB", CPU_DX, CPU_BX);
                CPU_AX = XMS_ERROR_CODE;
                CPU_BL = 0xA0; // no free space
                break;
            }
            CPU_BL = resize_xmm_page(CPU_DX, CPU_BX);
            CPU_AX = CPU_BL > 0x80 ? XMS_ERROR_CODE : XMS_SUCCESS_CODE;
#else
            CPU_BL = 0x86;
            CPU_AX = XMS_ERROR_CODE;
#endif
            sprintf(tmp, "XMS FN 0Fh: Resize Extended Memory Block #%d to %dKB", CPU_DX, CPU_BX);
            break;
#ifdef XMS_UMB
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
#endif
        default:
            sprintf(tmp, "XMS FN %2Xh: ERROR (not implemented)", CPU_AH);
            CPU_AX = XMS_ERROR_CODE; // ERROR
            CPU_BL = 0x80; // Function not implemented
        }
    }
    logMsg(tmp);
    return 0xCB /* CB RETF */;
}

void xmm_reboot() {
#ifdef XMS_UMB
    init_umb();
#endif
    hma_in_use_count = 0;
    xms_in_use = false;
#if XMS_OVER_HMA_KB
    memset(xmm_handles, 0, sizeof xmm_handles);
#endif
}

#endif
