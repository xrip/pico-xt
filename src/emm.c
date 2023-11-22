/*
 Based on
  https://github.com/bit-hack/fake86/blob/master/docs/limems40.txt


                                 LOTUS /INTEL /MICROSOFT

                              EXPANDED MEMORY SPECIFICATION

                                       Version 4.0

                                        300275-005

                                      October, 1987

          Copyright (c) 1987

          Lotus Development Corporation
          55 Cambridge Parkway
          Cambridge, MA 02142

          Intel Corporation
          5200 NE Elam Young Parkway
          Hillsboro, OR 97124

          Microsoft Corporation
          16011 NE 36TH Way
          Box 97017
          Redmond, WA 98073

          This specification was jointly developed by Lotus Development
          Corporation, Intel Corporation, and Microsoft Corporation.  Although
          it has been released into the public domain and is not confidential or
          proprietary, the specification is still the copyright and property of
          Lotus Development Corporation, Intel Corporation, and Microsoft
          Corporation.

          DISCLAIMER OF WARRANTY

          LOTUS DEVELOPMENT CORPORATION, INTEL CORPORATION, AND MICROSOFT
          CORPORATION EXCLUDE ANY AND ALL IMPLIED WARRANTIES, INCLUDING
          WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
          NEITHER LOTUS NOR INTEL NOR MICROSOFT MAKE ANY WARRANTY OF
          REPRESENTATION, EITHER EXPRESS OR IMPLIED, WITH RESPECT TO THIS
          SPECIFICATION, ITS QUALITY, PERFORMANCE, MERCHANTABILITY, OR FITNESS
          FOR A PARTICULAR PURPOSE.  NEITHER LOTUS NOR INTEL NOR MICROSOFT SHALL
          HAVE ANY LIABILITY FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
          ARISING OUT OF OR RESULTING FROM THE USE OR MODIFICATION OF THIS
          SPECIFICATION.

          This specification uses the following trademarks:

          Intel is a trademark of Intel Corporation
          Lotus is a trademark of Lotus Development Corporation
          Microsoft is a trademark of Microsoft Corporation
*/
#include "emm.h"
#include <string.h>

uint16_t emm_conventional_segment() {
    return PHISICAL_EMM_SEGMENT;
}

typedef __attribute__ ((__packed__)) struct emm_record {
    uint8_t handler;
    uint8_t phisical_page;
    uint16_t logical_page;
} emm_record_t;

static emm_record_t emm_desc_table[] = { 0 };

static uint8_t hanldle_desc[256] = { 0 }; // handlers are indexes
static uint8_t logical_page_desc[TOTAL_EMM_PAGES] = { 0 }; // number of handler for a page
static uint16_t phisical_page_n_2_logical[PHISICAL_EMM_PAGES] = { 0 }; // handlers are indexes

uint16_t total_open_emm_handles() {
    uint16_t res = 0;
    for (int i = 0; i < 256; ++i) {
        if (hanldle_desc[i]) res++;
    }
    return res;
}

uint16_t get_emm_handle_pages(uint16_t emm_handle, uint16_t *err) {
    if (emm_handle > 256 || hanldle_desc[emm_handle] == 0) {
        *err = 0x83 << 8; // The memory manager couldn't find the EMM handle your program specified.
        return 0;
    }
    uint16_t res = 0;
    for (int i = 0; i < TOTAL_EMM_PAGES; ++i) {
        if (logical_page_desc[i] == emm_handle) res++;
    }
    return res;
}

uint32_t get_logical_lba_for_phisical_lba(uint32_t physical_lba_addr) {
    uint16_t physical_page_number = physical_lba_addr >> 14;
    uint32_t offset_in_the_page = physical_lba_addr - (physical_page_number << 14);
    uint32_t logical_page_number = phisical_page_n_2_logical[physical_page_number - FIRST_PHISICAL_EMM_PAGE];
    if (logical_page_number == 0) {
        return 0xFFFFFFFF; // reserved as error
    }
    auto logical_page_offset = logical_page_number + (640 >> 4);
    auto logical_base_lba = logical_page_offset << 14;
    return logical_base_lba - offset_in_the_page;
}

uint16_t total_emm_pages() {
    return TOTAL_EMM_PAGES;
}

uint16_t allocated_emm_pages() {
    uint16_t res = 0;
    for (int i = 0; i < total_emm_pages(); ++i) {
        if (logical_page_desc[i] != 0) res++; 
    }
    return res;
}

uint16_t allocate_emm_pages(uint16_t pages, uint16_t *err) {
    if (pages == 0) {
        *err = 0x89 << 8; // Your program attempted to allocate zero pages.
        return 0;
    }
    if (pages > unallocated_emm_pages()) {
        *err = 0x87 << 8; // There aren't enough expanded memory pages present
        return 0;
    }
    for (int handler = 0; handler < 254; ++handler) {
        if (hanldle_desc[handler] == 0) {
            int pn = pages; // update pages descriptions - mark 'em as handled by handler
            for (int i = 0; i < total_emm_pages(); ++i) {
                if (logical_page_desc[i] != 0) {
                    logical_page_desc[i] = handler;
                    pn--;
                    if (pn == 0) {
                        break;
                    }
                }
            }
            if (pn > 0) {
                for (int i = 0; i < total_emm_pages(); ++i) { // rollback changes
                    if (logical_page_desc[i] == handler) {
                        logical_page_desc[i] = 0;
                    }
                }
                *err = 0x88 << 8; // There aren't enough unallocated pages
                return 0;               
            }
            *err = 0;
            hanldle_desc[handler] = pages;
            return handler;
        }
    }
    *err = 0x85 << 8; // All EMM handles are being used.
    return 0;
}

uint16_t map_unmap_emm_pages(
    uint8_t physical_page_number,
    uint16_t logical_page_number,
    uint16_t emm_handle
) {
    uint32_t base_lba = (uint32_t)emm_conventional_segment() << 4;
    uint32_t phisical_page_lba = (uint32_t)physical_page_number << 14;
    if (base_lba + (64ul << 10) >= phisical_page_lba) {
        return 0x88 << 8; // The physical page number is out of the range of allowable
                     // physical pages.  The program can recover by attempting to
                     // map into memory at a physical page which is within the
                     // range of allowable physical pages.
    }
    if (logical_page_number == 0xFFFF) { // if BX contains logical page number
                                         // FFFFh, the physical page will be unmapped
        phisical_page_n_2_logical[physical_page_number - FIRST_PHISICAL_EMM_PAGE] = 0; 
    }
    if (logical_page_number >= total_emm_pages() || logical_page_desc[logical_page_number] != emm_handle) {
        return 0x8A << 8; // The logical page is out of the range of logical pages which
                     // are allocated to the EMM handle. This status is also
                     // returned if a program attempts map a logical page when no
                     // logical pages are allocated to the handle.
    }
    phisical_page_n_2_logical[physical_page_number - FIRST_PHISICAL_EMM_PAGE] = logical_page_number;
    return 0;
}

uint16_t deallocate_emm_pages(uint16_t handler) {
/* TODO: detect the case
          AH = 86h   RECOVERABLE
                     The memory manager detected a save or restore page mapping
                     context error. There is a page mapping
                     register state in the save area for the specified EMM
                     handle. Save Page Map placed it there and a
                     subsequent Restore Page Map has not removed
                     it. If you have saved the mapping context, you must
                     restore it before you deallocate the EMM handle's pages.
*/
    for (int logical_page_number = 0; logical_page_number < total_emm_pages(); ++logical_page_number) { // rollback changes
        if (logical_page_desc[logical_page_number] == handler) {
            for (uint8_t phisical_page_offset = 0; phisical_page_offset < PHISICAL_EMM_PAGES; ++phisical_page_offset) {
                auto logical_page_number2 = phisical_page_n_2_logical[phisical_page_offset];
                if (logical_page_number2 == 0) {
                    return 0x86; // AH = 86h   RECOVERABLE
                    // The memory manager detected a save or restore page mapping
                    // context error. There is a page mapping
                    // register state in the save area for the specified EMM
                    // handle. Save Page Map placed it there and a
                    // subsequent Restore Page Map has not removed
                    // it. If you have saved the mapping context, you must
                    // restore it before you deallocate the EMM handle's pages.
                }
                if (logical_page_number == logical_page_number2) {
                    phisical_page_n_2_logical[phisical_page_offset] = 0;
                }
            }
            logical_page_desc[logical_page_number] = 0;
            break;
        }
    }
    hanldle_desc[handler] = 0;
    return 0;
}

static uint16_t saved_corr_id = 0;
static uint8_t hanldle_desc2[256] = { 0 }; // handlers are indexes
static uint8_t logical_page_desc2[TOTAL_EMM_PAGES] = { 0 }; // number of handler for a page
static uint16_t phisical_page_n_2_logical2[PHISICAL_EMM_PAGES] = { 0 }; // handlers are indexes

uint16_t save_emm_mapping(uint16_t corr_id) {
    if (saved_corr_id == corr_id) {
        return 0x8D << 8; // The save area already contains the page mapping register
                     // state for the EMM handle your program specified.

    }
    if (saved_corr_id != 0) {
        return 0x8C << 8; // There is no room in the save area to store the state of the
                     // page mapping registers.  The state of the map registers has
                     // not been saved.
    }
    memcpy(hanldle_desc2, hanldle_desc, sizeof hanldle_desc2);
    memcpy(logical_page_desc2, logical_page_desc, sizeof logical_page_desc2);
    memcpy(phisical_page_n_2_logical2, phisical_page_n_2_logical, sizeof phisical_page_n_2_logical2);
    return 0;
}

uint16_t restore_emm_mapping(uint16_t corr_id) {
    if (saved_corr_id != corr_id) {
        return 0x8E << 8; // There is no page mapping register state in the save area
                          // for the specified EMM handle.  Your program didn't save the
                          // contents of the page mapping hardware, so Restore Page Map
                          // can't restore it.
    }
    memcpy(hanldle_desc, hanldle_desc2, sizeof hanldle_desc2);
    memcpy(logical_page_desc, logical_page_desc2, sizeof logical_page_desc2);
    memcpy(phisical_page_n_2_logical, phisical_page_n_2_logical2, sizeof phisical_page_n_2_logical2);
    saved_corr_id = 0;
}

/*
handle_page_struct         STRUC
             emm_handle              DW ?
             pages_alloc_to_handle   DW ?
          handle_page_struct         ENDS

          ES:DI = pointer to handle_page
                     Contains a pointer to an array of structures where a copy
                     of all open EMM handles and the number of pages allocated
                     to each will be stored.  Each structure has these two
                     members:

          .emm_handle
                     The first member is a word which contains the value of the
                     open EMM handle.  The values of the handles this function
                     returns will be in the range of 0 to 255 decimal (0000h to
                     00FFh).  The uppermost byte of the handle is always zero.

          .pages_alloc_to_handle
                     The second member is a word which contains the number of
                     pages allocated to the open EMM handle.
*/
uint16_t get_all_emm_handle_pages(uint32_t addr32) {
    uint16_t res = 0;
    for(int handler = 0; handler < 256; ++handler) {
        if (hanldle_desc[handler] != 0) {
            res++;
            uint16_t pages_alloc_to_handle = 0;
            for (int j = 0; j < TOTAL_EMM_PAGES; ++j) {
                if (logical_page_desc[j] == handler) pages_alloc_to_handle++;
            }
            writew86(addr32++, handler);
            writew86(++addr32, pages_alloc_to_handle);
        }
    } 
    return res;
}

void get_emm_pages_map(uint32_t addr32) {
    for(int i = 0; i < 256; ++i) {
        write86(addr32++, hanldle_desc[i]);
    }
    for(int i = 0; i < TOTAL_EMM_PAGES; ++i) {
        write86(addr32++, logical_page_desc[i]);
    }
    for(int i = 0; i < PHISICAL_EMM_PAGES; ++i, addr32 += 2) {
        writew86(addr32, phisical_page_n_2_logical[i]);
    }
}

void set_emm_pages_map(uint32_t addr32) {
    for(int i = 0; i < 256; ++i) {
        hanldle_desc[i] = read86(addr32++);
    }
    for(int i = 0; i < TOTAL_EMM_PAGES; ++i) {
        logical_page_desc[i] = read86(addr32++);
    }
    for(int i = 0; i < PHISICAL_EMM_PAGES; ++i, addr32 += 2) {
        phisical_page_n_2_logical[i] = readw86(addr32++);;
    }
}

uint16_t get_emm_pages_map_size() {
    return 256;
}
