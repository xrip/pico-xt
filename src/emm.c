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

#define PHISICAL_EMM_SEGMENT 0xD000
#define PHISICAL_EMM_SEGMENT_KB 64
// pages by 16k
#define PHISICAL_EMM_PAGES (PHISICAL_EMM_SEGMENT_KB >> 4)
// PHISICAL_EMM_SEGMENT * 16 / 16k
#define FIRST_PHISICAL_EMM_PAGE (PHISICAL_EMM_SEGMENT >> 10)

uint16_t emm_conventional_segment() {
    return PHISICAL_EMM_SEGMENT;
}



static uint8_t hanldle_desc[256] = { 0 }; // handlers are indexes
static uint8_t page_desc[TOTAL_EMM_PAGES] = { 0 }; // number of handler for a page
static uint32_t ph_2_log[PHISICAL_EMM_PAGES] = { 0 }; // handlers are indexes

uint16_t total_emm_pages() {
    return TOTAL_EMM_PAGES;
}

uint16_t allocated_emm_pages() {
    uint16_t res = 0;
    for (int i = 0; i < total_emm_pages(); ++i) {
        if (page_desc[i] != 0) res++; 
    }
    return res;
}

uint16_t allocate_emm_pages(uint16_t pages, uint16_t *err) {
    if (pages == 0) {
        *err = 0x89; // Your program attempted to allocate zero pages.
        return 0;
    }
    if (pages > unallocated_emm_pages()) {
        *err = 0x87; // There aren't enough expanded memory pages present
        return 0;
    }
    for (int handler = 1; handler < 254; ++handler) {
        if (hanldle_desc[handler] == 0) {
            int pn = pages; // update pages descriptions - mark 'em as handled by handler
            for (int i = 0; i < total_emm_pages(); ++i) {
                if (page_desc[i] != 0) {
                    page_desc[i] = handler;
                    pn--;
                    if (pn == 0) {
                        break;
                    }
                }
            }
            if (pn > 0) {
                for (int i = 0; i < total_emm_pages(); ++i) { // rollback changes
                    if (page_desc[i] != handler) {
                        page_desc[i] = 0;
                    }
                }
                *err = 0x88; // There aren't enough unallocated pages
                return 0;               
            }
            *err = 0;
            hanldle_desc[handler] = pages;
            return handler;
        }
    }
    *err = 0x85; // All EMM handles are being used.
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
        return 0x88; // The physical page number is out of the range of allowable
                     // physical pages.  The program can recover by attempting to
                     // map into memory at a physical page which is within the
                     // range of allowable physical pages.
    }
    if (logical_page_number == 0xFFFF) { // if BX contains logical page number
                                         // FFFFh, the physical page will be unmapped
        ph_2_log[physical_page_number - FIRST_PHISICAL_EMM_PAGE] = 0; 
    }
    if (logical_page_number >= total_emm_pages() || page_desc[logical_page_number] != emm_handle) {
        return 0x8A; // The logical page is out of the range of logical pages which
                     // are allocated to the EMM handle. This status is also
                     // returned if a program attempts map a logical page when no
                     // logical pages are allocated to the handle.
    }
    ph_2_log[physical_page_number - FIRST_PHISICAL_EMM_PAGE] = logical_page_number;

    return 0;
}
