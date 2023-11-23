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

typedef emm_record_t emm_desc_table_t[PHISICAL_EMM_PAGES];

static emm_desc_table_t emm_desc_table = { 0 };

uint16_t total_open_emm_handles() {
    uint16_t res = 0;
    for (int i = 0; i < PHISICAL_EMM_PAGES; ++i) {
        if (emm_desc_table[i].handler) res++;
    }
    return res;
}

uint16_t get_emm_handle_pages(uint16_t emm_handle, uint16_t *err) {
    uint16_t res = 0;
    for (int i = 0; i < PHISICAL_EMM_PAGES; ++i) {
        if (emm_desc_table[i].handler == emm_handle) res++;
    }
    if (res == 0) {
        *err = 0x83 << 8; // The memory manager couldn't find the EMM handle your program specified.
        return 0;
    }
    return res;
}

uint32_t get_logical_lba_for_phisical_lba(uint32_t physical_lba_addr) {
    uint16_t physical_page_number = physical_lba_addr >> 14;
    uint32_t offset_in_the_page = physical_lba_addr - (physical_page_number << 14);
    for (int i = 0; i < PHISICAL_EMM_PAGES; ++i) {
        const emm_record_t * di = &emm_desc_table[i];
        if (di->phisical_page == physical_page_number) {
            uint32_t logical_page_number = di->logical_page;
            auto logical_base_lba = logical_page_number << 14;
            return logical_base_lba - offset_in_the_page;
        }
    }
    return 0xFFFFFFFF; // reserved as error
}

uint16_t total_emm_pages() {
    return TOTAL_EMM_PAGES;
}

uint16_t allocated_emm_pages() {
    return total_open_emm_handles();
}

uint16_t allocate_emm_pages(uint16_t pages, uint16_t *err) {
    if (pages == 0) {
        *err = 0x89 << 8; // Your program attempted to allocate zero pages.
        return 0;
    }
    if (pages > PHISICAL_EMM_PAGES) {
        *err = 0x87 << 8; // There aren't enough expanded memory pages present
        return 0;
    }
    uint16_t handler = 0xFFFF;
    int cnt = 0;
    int i = 0;
    for (; i < PHISICAL_EMM_PAGES && cnt < pages; ++i) {
        emm_record_t * di = &emm_desc_table[i];
        if(di->handler == 0) {
            if (handler == 0xFFFF) {
                handler = i + 1;
            }
            di->handler = handler; // update pages descriptions - mark 'em as handled by handler
            cnt++;
        }
    }
    if (cnt != pages) {  // rollback changes
        for (int i = 0; i < PHISICAL_EMM_PAGES; ++i) {
            emm_record_t * di = &emm_desc_table[i];
            if (di->handler == handler) di->handler = 0;
        }
        *err = (i == pages) ?
               (0x85 << 8) : // All EMM handles are being used.
               (0x88 << 8); // There aren't enough unallocated pages
        return 0;  
    }
    *err = 0;
    return handler;
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
        for (int i = 0; i < PHISICAL_EMM_PAGES; ++i) {
            emm_record_t * di = &emm_desc_table[i];
            if (di->phisical_page == physical_page_number && di->handler == emm_handle) {
                di->phisical_page = 0;
                di->logical_page = 0;
            }
        }
        return 0;
    }
    if (logical_page_number >= total_emm_pages()) {
        return 0x8A << 8; // The logical page is out of the range of logical pages which
                     // are allocated to the EMM handle. This status is also
                     // returned if a program attempts map a logical page when no
                     // logical pages are allocated to the handle.
    }
    for (int i = 0; i < PHISICAL_EMM_PAGES; ++i) {
        emm_record_t * di = &emm_desc_table[i];
        if (di->handler == emm_handle) {
            di->phisical_page = physical_page_number;
            di->logical_page = logical_page_number;
        }
    }
    return 0;
}

uint16_t deallocate_emm_pages(uint16_t emm_handle) {
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
    for (int i = 0; i < PHISICAL_EMM_PAGES; ++i) {
        emm_record_t * di = &emm_desc_table[i];
        if (di->handler == emm_handle) {
            if (di->phisical_page != 0 || di->logical_page != 0) {
                return 0x86; // AH = 86h   RECOVERABLE
                    // The memory manager detected a save or restore page mapping
                    // context error. There is a page mapping
                    // register state in the save area for the specified EMM
                    // handle. Save Page Map placed it there and a
                    // subsequent Restore Page Map has not removed
                    // it. If you have saved the mapping context, you must
                    // restore it before you deallocate the EMM handle's pages.
            } else {
                di->handler = 0;
            }
        }
    }
    return 0;
}

#define MAX_SAVED_TABLES 4

typedef struct emm_saved_table {
    uint16_t ext_handler;
    emm_desc_table_t table;
} emm_saved_table_t;

static emm_saved_table_t emm_saved_tables[MAX_SAVED_TABLES] = { 0 };

uint16_t save_emm_mapping(uint16_t ext_handler) {
    emm_saved_table_t * to_save = 0;
    for (int i = 0; i < MAX_SAVED_TABLES; ++i) {
        emm_saved_table_t * di = &emm_saved_tables[i];
        if (di->ext_handler == ext_handler) {
            return 0x8D << 8; // The save area already contains the page mapping register
                              // state for the EMM handle your program specified.
        }
        if (to_save == 0 && di->ext_handler == 0) {
            to_save = di;
        }
    }
    if (to_save == 0) {
        return 0x8C << 8; // There is no room in the save area to store the state of the
                     // page mapping registers.  The state of the map registers has
                     // not been saved.
    }
    memcpy(to_save->table, emm_desc_table, sizeof emm_desc_table);
    return 0;
}

uint16_t restore_emm_mapping(uint16_t ext_handler) {
    for (int i = 0; i < MAX_SAVED_TABLES; ++i) {
        emm_saved_table_t * di = &emm_saved_tables[i];
        if (di->ext_handler == ext_handler) {
            memcpy(emm_desc_table, di->table, sizeof emm_desc_table);
            di->ext_handler = 0;
            return 0;
        }
    }
    return 0x8E << 8; // There is no page mapping register state in the save area
                      // for the specified EMM handle.  Your program didn't save the
                      // contents of the page mapping hardware, so Restore Page Map
                      // can't restore it.
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
    for (int i = 0; i < PHISICAL_EMM_PAGES; ++i) {
        const emm_record_t * di = &emm_desc_table[i];
        if (di->handler == 0)
            continue;
        ++res;
        uint16_t pages_alloc_to_handle = 0;
        for (int j = 0; j < PHISICAL_EMM_PAGES; ++j) {
            const emm_record_t * dj = &emm_desc_table[j];
            if (di->handler == dj->handler) {
                ++pages_alloc_to_handle;
            }
        }
        writew86(addr32++, di->handler);
        writew86(++addr32, pages_alloc_to_handle);
    }
    return res;
}

void get_emm_pages_map(uint32_t addr32) {
    uint8_t * t = &emm_desc_table;
    for (int i = 0; i < sizeof emm_desc_table; ++i) {
        write86(addr32++, *t++);
    }
}

void set_emm_pages_map(uint32_t addr32) {
    uint8_t * t = &emm_desc_table;
    for (int i = 0; i < sizeof emm_desc_table; ++i) {
        *t++ = read86(addr32++);
    }
}

uint16_t get_emm_pages_map_size() {
    return sizeof emm_desc_table;
}

/*
          partial_page_map_struct     STRUC
             mappable_segment_count   DW  ?
             mappable_segment         DW  (?)  DUP  (?)
          partial_page_map_struct     ENDS

          DS:SI = partial_page_map
                     Contains a pointer to a structure which specifies only
                     those mappable memory regions which are to have their
                     mapping context saved.  The structure members are described
                     below.

          .mappable_segment_count
                     The first member is a word which specifies the number of
                     members in the word array which immediately follows it.
                     This number should not exceed the number of mappable
                     segments in the system.

          .mappable_segment
                     The second member is a word array which contains the
                     segment addresses of the mappable memory regions whose
                     mapping contexts are to be saved.  The segment address must
                     be a mappable segment.  Use Function 25 to determine which
                     segments are mappable.

          ES:DI = dest_array
                     Contains a pointer to the destination array address in
                     Segment:Offset format.  To determine the size of the
                     required array, see the Get Size of Partial Page Map Save
                     Array subfunction.
*/
uint16_t get_partial_emm_page_map(uint32_t partial_page_map, uint32_t dest_array) {
   // TODO:
   return 0x86 << 8;
}
