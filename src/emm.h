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

#define ON_BOARD_RAM_KB (4ul << 10)
#define BASE_X86_KB 1024ul
#define TOTAL_XMM_KB (ON_BOARD_RAM_KB - BASE_X86_KB)
#define TOTAL_EMM_KB (4ul << 10)
#define EMM_LBA_SHIFT_KB ON_BOARD_RAM_KB
#define TOTAL_EMM_PAGES (TOTAL_EMM_KB >> 4)
#define TOTAL_VIRTUAL_MEMORY_KBS (ON_BOARD_RAM_KB + TOTAL_EMM_KB)

#define PHYSICAL_EMM_SEGMENT 0xD000
#define PHYSICAL_EMM_SEGMENT_KB 64
#define PHYSICAL_EMM_SEGMENT_END (PHYSICAL_EMM_SEGMENT + 0x1000)
// pages by 16k
#define PHYSICAL_EMM_PAGES (PHYSICAL_EMM_SEGMENT_KB >> 4)
// PHYSICAL_EMM_SEGMENT * 16 / 16k
#define FIRST_PHYSICAL_EMM_PAGE (PHYSICAL_EMM_SEGMENT >> 10)

#define MAX_SAVED_EMM_TABLES 4
#define MAX_EMM_HANDLERS 255
#define MAX_EMM_HANDLER_NAME_SZ 8

void init_emm();
uint16_t emm_conventional_segment();
uint16_t total_emm_pages();
uint16_t allocated_emm_pages();
uint16_t allocate_emm_pages(uint16_t pages, uint16_t *err);
uint16_t reallocate_emm_pages(uint16_t handler, uint16_t pages);
static inline uint16_t unallocated_emm_pages() {
    return total_emm_pages() - allocated_emm_pages();
}

uint16_t map_unmap_emm_page(
    uint8_t physical_page_number,
    uint16_t logical_page_number,
    uint16_t emm_handle
);
uint16_t deallocate_emm_pages(uint16_t emm_handler);
uint32_t get_logical_lba_for_physical_lba(uint32_t addr32);
uint16_t total_open_emm_handles();
uint16_t get_emm_handle_pages(uint16_t emm_handle, uint16_t *err);
uint16_t save_emm_mapping(uint16_t corr_id);
uint16_t restore_emm_mapping(uint16_t corr_id);
uint16_t get_all_emm_handle_pages(uint32_t addr32);
void get_emm_pages_map(uint32_t addr32);
void set_emm_pages_map(uint32_t addr32);
uint16_t get_emm_pages_map_size();
uint16_t get_partial_emm_page_map(uint32_t partial_page_map, uint32_t dest_array);
uint16_t set_partial_emm_page_map(uint32_t dest_array);
// from cpu.c
void writew86(uint32_t addr32, uint16_t value);
void write86(uint32_t addr32, uint8_t value);
uint16_t readw86(uint32_t addr32);
uint8_t read86(uint32_t addr32);

uint16_t map_unmap_emm_pages(uint16_t handle, uint16_t log_to_phys_map_len, uint32_t log_to_phys_map);
uint16_t map_unmap_emm_seg_pages(uint16_t handle, uint16_t log_to_seg_map_len, uint32_t log_to_segment_map);
uint16_t get_mappable_physical_array(uint16_t mappable_phys_page);
uint16_t get_mappable_phys_pages();
uint16_t get_handle_name(uint16_t handle, uint32_t name);
uint16_t set_handle_name(uint16_t handle, uint32_t name);
uint16_t get_handle_dir(uint32_t handle_dir_struct);
uint16_t lookup_handle_dir(uint32_t handle_name, uint16_t *AH);
uint8_t map_emm_and_jump(uint8_t page_number_segment_selector, uint16_t handle, uint32_t map_and_jump);
uint8_t map_emm_and_call(uint8_t page_number_segment_selector, uint16_t handle, uint32_t map_and_call);
void get_hardvare_emm_info(uint32_t hardware_info);
uint16_t allocate_emm_pages_sys(uint16_t handler, uint16_t pages);
uint16_t allocate_emm_raw_pages(uint16_t pages);

void custom_on_board_emm();
