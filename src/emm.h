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
#pragma once
#include <inttypes.h>

#define PSEUDO_RAM_SIZE_MBS 32UL
#define TOTAL_VIRTUAL_MEMORY_KBS (PSEUDO_RAM_SIZE_MBS << 10)
#define TOTAL_EMM_XMM_PAGES ((TOTAL_VIRTUAL_MEMORY_KBS - 640) / 16)
#define TOTAL_EMM_PAGES TOTAL_EMM_XMM_PAGES
// TODO: XMM pages - to a20.h ?
#define PHISICAL_EMM_SEGMENT 0xD000
#define PHISICAL_EMM_SEGMENT_KB 64
#define PHISICAL_EMM_SEGMENT_END 0xE000
// pages by 16k
#define PHISICAL_EMM_PAGES (PHISICAL_EMM_SEGMENT_KB >> 4)
// PHISICAL_EMM_SEGMENT * 16 / 16k
#define FIRST_PHISICAL_EMM_PAGE (PHISICAL_EMM_SEGMENT >> 10)

#define MAX_SAVED_EMM_TABLES 1
#define MAX_EMM_HANDLERS 255

void init_emm();
uint16_t emm_conventional_segment();
uint16_t total_emm_pages();
uint16_t allocated_emm_pages();
uint16_t allocate_emm_pages(uint16_t pages, uint16_t *err);

static inline uint16_t unallocated_emm_pages() {
    return total_emm_pages() - allocated_emm_pages();
}

uint16_t map_unmap_emm_pages(
    uint8_t physical_page_number,
    uint16_t logical_page_number,
    uint16_t emm_handle
);
uint16_t deallocate_emm_pages(uint16_t emm_handler);
uint32_t get_logical_lba_for_phisical_lba(uint32_t addr32);
uint16_t total_open_emm_handles();
uint16_t get_emm_handle_pages(uint16_t emm_handle, uint16_t *err);
uint16_t save_emm_mapping(uint16_t corr_id);
uint16_t restore_emm_mapping(uint16_t corr_id);
uint16_t get_all_emm_handle_pages(uint32_t addr32);
void get_emm_pages_map(uint32_t addr32);
void set_emm_pages_map(uint32_t addr32);
uint16_t get_emm_pages_map_size();
uint16_t get_partial_emm_page_map(uint32_t partial_page_map, uint32_t dest_array);
// from cpu.c
void writew86(uint32_t addr32, uint16_t value);
void write86(uint32_t addr32, uint8_t value);
uint16_t readw86(uint32_t addr32);
uint8_t read86(uint32_t addr32);
