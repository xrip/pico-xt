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
#include <stdbool.h>

uint16_t emm_conventional_segment() {
    return PHYSICAL_EMM_SEGMENT;
}

typedef __attribute__ ((__packed__)) struct emm_record {
    uint8_t handler;
    uint8_t physical_page;
    uint16_t logical_page;
} emm_record_t;

typedef emm_record_t emm_desc_table_t[PHYSICAL_EMM_PAGES];

typedef struct emm_saved_table {
    uint16_t ext_handler;
    emm_desc_table_t table;
} emm_saved_table_t;

static emm_saved_table_t emm_saved_tables[MAX_SAVED_EMM_TABLES] = { 0 };

static emm_desc_table_t emm_desc_table = { 0 };

typedef __attribute__ ((__packed__)) struct emm_handler {
    uint8_t handler_in_use;
    uint8_t pages_acllocated;
    char name[MAX_EMM_HANDLER_NAME_SZ];
} emm_handler_t;

static emm_handler_t handlers[MAX_EMM_HANDLERS] = { 0 };

void init_emm() {
    emm_handler_t * h = &handlers[0];
    h->handler_in_use = true;
    h->pages_acllocated = 0;
    for (int i = 1; i < MAX_EMM_HANDLERS; ++i) {
        h = &handlers[i];
        h->handler_in_use = false;
        h->pages_acllocated = 0;
    }
    for (int i = 0; i < h->pages_acllocated; ++i) {
        emm_record_t * di = &emm_desc_table[i];
        di->handler = 0x00;
        di->logical_page = i;
        di->physical_page = i;
    }
    for (int i = h->pages_acllocated; i < PHYSICAL_EMM_PAGES; ++i) {
        emm_record_t * di = &emm_desc_table[i];
        di->handler = 0xFF;
        di->logical_page = i;
        di->physical_page = i;
    }
    for (int j = 0; j < MAX_SAVED_EMM_TABLES; ++j) {
        emm_saved_table_t * st = &emm_saved_tables[j];
        st->ext_handler = 0;
        for (int i = 0; i < PHYSICAL_EMM_PAGES; ++i) {
            emm_record_t * di = &st->table[i];
            di->handler = 0xFF;
            di->logical_page = 0;
            di->physical_page = 0;
        }
    }
}

uint16_t total_open_emm_handles() {
    uint16_t res = 0;
    for (int i = 0; i < MAX_EMM_HANDLERS; ++i) {
        if (handlers[i].handler_in_use) res++;
    }
    return res;
}

uint16_t get_emm_handle_pages(uint16_t emm_handle, uint16_t *err) {
    if (emm_handle >= MAX_EMM_HANDLERS || handlers[emm_handle].handler_in_use == 0) {
        *err = 0x83 << 8; // The memory manager couldn't find the EMM handle your program specified.
        return 0;
    }
    *err = 0;
    return handlers[emm_handle].pages_acllocated;
}

uint32_t get_logical_lba_for_physical_lba(uint32_t physical_lba_addr) {
    uint16_t physical_page_number = (physical_lba_addr >> 14);
    uint32_t offset_in_the_page = physical_lba_addr - (physical_page_number << 14);
    for (int i = 0; i < PHYSICAL_EMM_PAGES; ++i) {
        const emm_record_t * di = &emm_desc_table[i];
        if (di->physical_page + (PHYSICAL_EMM_SEGMENT >> 10) == physical_page_number && di->handler != 0xFF) {
            uint32_t logical_page_number = di->logical_page;
            uint32_t logical_base_lba = logical_page_number << 14;
            return logical_base_lba + offset_in_the_page + (EMM_LBA_SHIFT_KB << 10); // shift to do not intersect with on board RAM
        }
    }
    return physical_lba_addr; // do not map not found
}

uint16_t total_emm_pages() {
    return TOTAL_EMM_PAGES;
}

uint16_t allocated_emm_pages() {
    uint16_t res = 0;
    for (int i = 0; i < MAX_EMM_HANDLERS; ++i) {
        emm_handler_t * h = &handlers[i];
        if (h->handler_in_use) {
            res += h->pages_acllocated;
        }
    }
    return res;
}

// allocate doesn't not mean "map", so just reserv space-pages as "allocated"
uint16_t allocate_emm_pages(uint16_t pages, uint16_t *err) {
    if (pages == 0) {
        *err = 0x89 << 8; // Your program attempted to allocate zero pages.
        return 0;
    }
    if (pages > TOTAL_EMM_PAGES) {
        *err = 0x87 << 8; // There aren't enough expanded memory pages present
        return 0;
    }
    if (pages + allocated_emm_pages() > TOTAL_EMM_PAGES) {
        *err = 0x88 << 8; // There aren't enough unallocated pages
        return 0;
    }
    uint16_t handler = 0xFFFF;
    /*
          The numeric value of the handles the EMM returns are in the range of 1
          to 254 decimal (0001h to 00FEh).  The OS handle (handle value 0) is
          never returned by the Allocate Pages function.  Also, the uppermost
          byte of the handle will be zero and cannot be used by the application.
          A memory manager should be able to supply up to 255 handles, including
          the OS handle.  An application can use Function 21 to find out how
          many handles an EMM supports.
    */
    for (int i = 1; i < MAX_EMM_HANDLERS; ++i) {
        emm_handler_t * h = &handlers[i];
        if (!h->handler_in_use) {
            handler = i;
            h->handler_in_use = true;
            h->pages_acllocated = pages;
            break;
        }
    }
    if (handler == 0xFFFF) {
        *err = 0x85 << 8; // All EMM handles are being used.
        return 0xFF;          
    }
    *err = 0;
    return handler;
}

uint16_t reallocate_emm_pages(uint16_t handler, uint16_t pages) {
    if (handler >= MAX_EMM_HANDLERS || handlers[handler].handler_in_use == 0) {
        return 0x83 << 8; // The memory manager couldn't find the EMM handle your program specified.
    }
    if (pages > TOTAL_EMM_PAGES) {
        return 0x87 << 8; // There aren't enough expanded memory pages present
    }
    if (handlers[handler].pages_acllocated == pages) {
        return 0;
    }
    if (handlers[handler].pages_acllocated > pages) {
        if (handlers[handler].pages_acllocated - pages + allocated_emm_pages() > TOTAL_EMM_PAGES) {
            return 0x88 << 8; // There aren't enough unallocated pages
        }
    }
    handlers[handler].pages_acllocated = pages;
    return 0;
}

static inline uint32_t phisical_page_2_lba(uint32_t physical_page_number) {
    return (physical_page_number << 14) + (emm_conventional_segment() << 4);
}

uint16_t map_unmap_emm_page(
    uint8_t physical_page_number,
    uint16_t logical_page_number,
    uint16_t emm_handle
) {
    uint32_t physical_page_lba = phisical_page_2_lba(physical_page_number);
    if (physical_page_lba >= ((uint32_t)PHYSICAL_EMM_SEGMENT_END << 4)) { // TODO: ensure
        return 0x88 << 8; // The physical page number is out of the range of allowable
                          // physical pages.  The program can recover by attempting to
                          // map into memory at a physical page which is within the
                          // range of allowable physical pages.
    }
    if (logical_page_number == 0xFFFF) { // if BX contains logical page number
                                         // FFFFh, the physical page will be unmapped
        for (int i = 0; i < PHYSICAL_EMM_PAGES; ++i) {
            emm_record_t * di = &emm_desc_table[i];
            if (di->physical_page == physical_page_number && di->handler == emm_handle) {
                di->handler = 0xFF;
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
    for (int i = 0; i < PHYSICAL_EMM_PAGES; ++i) {
        emm_record_t * di = &emm_desc_table[i];
        if (di->handler == 0xFF) { // empty line
            di->physical_page = physical_page_number;
            di->logical_page = logical_page_number;
            di->handler = emm_handle;
            break;
        }
    }
    return 0;
}

uint16_t deallocate_emm_pages(uint16_t emm_handle) {
    if (emm_handle >= MAX_EMM_HANDLERS) {
        return 0x8300; // The manager couldn't find the specified EMM handle.
    }
    emm_handler_t * h = &handlers[emm_handle];
    if (!h->handler_in_use) {
        return 0x8300; // The manager couldn't find the specified EMM handle.
    }
    for (int j = 0; j < MAX_SAVED_EMM_TABLES; ++j) {
        const emm_saved_table_t * st = &emm_saved_tables[j];
        for (int i = 0; i < PHYSICAL_EMM_PAGES; ++i) {
            const emm_record_t * di = &st->table[i];
            if (di->handler == emm_handle) {
                return 0x86 << 8; // There is a page mapping
                                  // register state in the save area for the specified EMM
                                  // handle. Save Page Map placed it there and a
                                  // subsequent Restore Page Map has not removed
                                  // it. If you have saved the mapping context, you must
                                  // restore it before you deallocate the EMM handle's pages.
            }
        }
    }
    for (int i = 0; i < PHYSICAL_EMM_PAGES; ++i) {
        emm_record_t * di = &emm_desc_table[i];
        if (di->handler == emm_handle) {
            // return 0x86 << 8; // The memory manager detected a save or restore page mapping context error.
            di->handler = 0xFF;
        }
    }
    h->pages_acllocated = 0;
    if (emm_handle) // do not mark zero handler as not in use
        h->handler_in_use = false;
    return 0;
}

uint16_t save_emm_mapping(uint16_t ext_handler) {
    emm_saved_table_t * to_save = 0;
    for (int i = 0; i < MAX_SAVED_EMM_TABLES; ++i) {
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
    to_save->ext_handler = ext_handler;
    memcpy(to_save->table, emm_desc_table, sizeof emm_desc_table);
    return 0;
}

uint16_t restore_emm_mapping(uint16_t ext_handler) {
    for (int i = 0; i < MAX_SAVED_EMM_TABLES; ++i) {
        emm_saved_table_t * di = &emm_saved_tables[i];
        if (di->ext_handler == ext_handler) {
            memcpy(emm_desc_table, di->table, sizeof emm_desc_table);
            memset(di->table, 0, sizeof emm_desc_table);
            di->ext_handler = 0;
            return 0;
        }
    }
    return 0x8E << 8; // There is no page mapping register state in the save area
                      // for the specified EMM handle.  Your program didn't save the
                      // contents of the page mapping hardware, so Restore Page Map
                      // can't restore it.
}

/*        handle_page_struct         STRUC
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
    for (int handler = 0; handler < MAX_EMM_HANDLERS; ++handler) {
        emm_handler_t * h = &handlers[handler];
        if (!h->handler_in_use)
            continue;
        ++res;
        writew86(addr32++, handler); addr32++;
        writew86(addr32++, h->pages_acllocated); addr32++;
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

/*        partial_page_map_struct     STRUC
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
   return get_all_emm_handle_pages(dest_array);
   // TODO: W/A ^ It should be implemented by addresses from page map instead
}

uint16_t set_partial_emm_page_map(uint32_t dest_array) {
   return save_emm_mapping(dest_array);
   // TODO: W/A ^ It should be implemented by addresses from page map instead
}
/*
          log_to_phys_map_struct   STRUC
             log_page_number       DW  ?
             phys_page_number      DW  ?
          log_to_phys_map_struct   ENDS

          DX = handle
                     Contains the EMM handle.
          CX = log_to_phys_map_len
                     Contains the number of entries in the array.  For example,
                     if the array contained four pages to map or unmap, then CX
                     would contain 4.  The number in CX should not exceed the
                     number of mappable pages in the system.
          DS:SI = pointer to log_to_phys_map array
                     Contains a pointer to an array of structures that contains
                     the information necessary to map the desired pages.  The
                     array is made up of the following two elements:
          .log_page_number
                     The first member is a word which contains the number of the
                     logical page which is to be mapped.  Logical pages are
                     numbered zero relative, so the number for a logical page
                     can only range from zero to (maximum number of logical
                     pages allocated to the handle - 1).
                     If the logical page number is set to FFFFh, the physical
                     page associated with it is unmapped rather than mapped.
                     Unmapping a physical page makes it inaccessible for reading
                     or writing.
          .phys_page_number
                     The second member is a word which contains the number of
                     the physical page at which the logical page is to be
                     mapped.  Physical pages are numbered zero relative, so the
                     number for a physical page can only range from zero to
                     (maximum number of physical pages supported in the system - 1).
*/
uint16_t map_unmap_emm_pages(uint16_t handle, uint16_t log_to_phys_map_len, uint32_t log_to_phys_map) {
    for (int i = 0; i < log_to_phys_map_len; ++i) {
        uint16_t log_page_number  = readw86(log_to_phys_map++); log_to_phys_map++;
        uint16_t phys_page_number = readw86(log_to_phys_map++); log_to_phys_map++;
        uint16_t res = map_unmap_emm_page(phys_page_number, log_page_number, handle); // W/A TODO: with rollback
        if (res) {
            return res;
        }
    }
    return 0;
}
/*
                     Contains the Map/Unmap Multiple Handle Pages subfunction
                     using the logical page/segment address method.

          log_to_seg_map_struct         STRUC
             log_page_number            DW  ?
             mappable_segment_address   DW  ?
          log_to_seg_map_struct         ENDS

          DX = handle
                     Contains the EMM handle.
          CX = log_to_segment_map_len
                     Contains the number of entries in the array.  For example,
                     if the array contained four pages to map or unmap, then CX
                     would contain four.
          DS:SI = pointer to log_to_segment_map array
                     Contains a pointer to an array of structures that contains
                     the information necessary to map the desired pages.  The
                     array is made up of the following elements:
          .log_page_number
                     The first member is a word which contains the number of the
                     logical pages to be mapped.  Logical pages are numbered
                     zero relative, so the number for a logical page can range
                     from zero to (maximum number of logical pages allocated to
                     the handle - 1).
                     If the logical page number is set to FFFFh, the physical
                     page associated with it is unmapped rather than mapped.
                     Unmapping a physical page makes it inaccessible for reading
                     or writing.
          .mappable_segment_address
                     The second member is a word which contains the segment
                     address at which the logical page is to be mapped.  This
                     segment address must correspond exactly to a mappable
                     segment address.  The mappable segment addresses are
                     available with Function 25 (Get Mappable Physical Address
                     Array)
*/
uint16_t map_unmap_emm_seg_pages(uint16_t handle, uint16_t log_to_seg_map_len, uint32_t log_to_segment_map) {
    for (int i = 0; i < log_to_seg_map_len; ++i) {
        uint16_t log_page_number  = readw86(log_to_segment_map++); log_to_segment_map++;
        uint16_t mappable_segment_address = readw86(log_to_segment_map++); log_to_segment_map++;
        uint16_t phys_page_number = mappable_segment_address >> 10;
        uint16_t res = map_unmap_emm_page(phys_page_number, log_page_number, handle); // W/A TODO: with rollback
        if (res) {
            return res;
        }
    }
    return 0;
}
/*
          This subfunction returns an array containing the segment address and
          physical page number for each mappable physical page in a system.  The
          contents of this array provide a cross reference between physical page
          numbers and the actual segment addresses for each mappable page in the
          system.  The array is sorted in ascending segment order.  This does
          not mean that the physical page numbers associated with the segment
          addresses are also in ascending order.

          CALLING PARAMETERS

          AX = 5800h
                     Contains the Get Mappable Physical Address Array
                     subfunction.

          mappable_phys_page_struct   STRUC
             phys_page_segment        DW ?
             phys_page_number         DW ?
          mappable_phys_page_struct   ENDS

          ES:DI = mappable_phys_page
                     Contains a pointer to an application-supplied memory area
                     where the memory manager will copy the physical address
                     array.  Each entry in the array is a structure containing
                     two members:
          .phys_page_segment
                     The first member is a word which contains the segment
                     address of the mappable physical page associated with the
                     physical page number following it.  The array entries are
                     sorted in ascending segment address order.
          .phys_page_number
                     The second member is a word which contains the physical
                     page number which corresponds to the previous segment
                     address.  The physical page numbers are not necessarily in
                     ascending order.

*/
uint16_t get_mappable_phys_pages() { return 4; }
uint16_t get_mappable_physical_array(uint16_t mappable_phys_page) {
    char tmp[80];
    uint16_t cs = PHYSICAL_EMM_SEGMENT;
    for (uint16_t physical_page = 0; physical_page < get_mappable_phys_pages(); ++physical_page, cs += 0x400) {
        writew86(mappable_phys_page++, cs); mappable_phys_page++;
        writew86(mappable_phys_page++, physical_page); mappable_phys_page++;
        sprintf(tmp, "LIM40 PAGE 0x%X : %d", cs, physical_page); logMsg(tmp);
    }
    return 0;
}

uint16_t get_handle_name(uint16_t handle, uint32_t name) {
    if (handle >= MAX_EMM_HANDLERS || handlers[handle].handler_in_use == 0) {
        return 0x8300; // The memory manager couldn't find the EMM handle your program specified.
    }
    char* hn = handlers[handle].name;
    for (int i = 0; i < MAX_EMM_HANDLER_NAME_SZ; ++i) {
        write86(name++, hn[i]);
    }
    return 0;
}
uint16_t set_handle_name(uint16_t handle, uint32_t name) {
    if (handle >= MAX_EMM_HANDLERS || handlers[handle].handler_in_use == 0) {
        return 0x8300; // The memory manager couldn't find the EMM handle your program specified.
    }
    char* hn = handlers[handle].name; // TODO: unique
    for (int i = 0; i < MAX_EMM_HANDLER_NAME_SZ; ++i) {
        hn[i] = read86(name++);
    }
    return 0;
}

//  handle_dir_struct   STRUC
//     handle_value     DW  ?
//     handle_name      DB  8  DUP  (?)
//  handle_dir_struct   ENDS
uint16_t get_handle_dir(uint32_t handle_dir_struct) {
    uint16_t res = 0;
    for (uint16_t handler = 0; handler < MAX_EMM_HANDLERS; ++handler) {
        emm_handler_t* h = &handlers[handler];
        if (h->handler_in_use == 0) continue;
        writew86(handle_dir_struct++, handler); handle_dir_struct++;
        char* hn = h->name;
        for (int i = 0; i < MAX_EMM_HANDLER_NAME_SZ; ++i) {
            write86(handle_dir_struct++, hn[i]);
        }
        res++;
    }
    return res;
}

uint16_t lookup_handle_dir(uint32_t handle_name, uint16_t *AH) {
    char hn[8];
    for (int i = 0; i < MAX_EMM_HANDLER_NAME_SZ; ++i) {
        hn[i] = read86(handle_name++);
    }
    for (uint16_t handler = 0; handler < MAX_EMM_HANDLERS; ++handler) {
        emm_handler_t* h = &handlers[handler];
        if (h->handler_in_use && (*((uint64_t*)h->name)) == (*((uint64_t*)hn))) {
            *AH = 0;
            return handler;
        }
    }
    *AH = 0xA0; // No corresponding handle could be found for the handle name specified.
    return 0xFF;
}

/*
          log_phys_map_struct       STRUC
             log_page_number        DW ?
             phys_page_number_seg   DW ?
          log_phys_map_struct       ENDS

          map_and_call_struct       STRUC
             target_address         DD ?
             new_page_map_len       DB ?
             new_page_map_ptr       DD ?
             old_page_map_len       DB ?
             old_page_map_ptr       DD ?
             reserved               DW  4 DUP (?)
          map_and_call_struct       ENDS
*/
uint8_t map_emm_and_jump(
    uint8_t page_number_segment_selector,
    uint16_t handle,
    uint32_t map_and_jump
) {
// TODO:
    return 0x86;
}

uint8_t map_emm_and_call(
    uint8_t page_number_segment_selector,
    uint16_t handle,
    uint32_t map_and_call
) {
// TODO:
    return 0x86;
}

/*
          hardware_info_struct         STRUC
             raw_page_size             DW ?
             alternate_register_sets   DW ?
             context_save_area_size    DW ?
             DMA_register_sets         DW ?
             DMA_channel_operation     DW ?
          hardware_info_struct         ENDS
*/
void get_hardvare_emm_info(uint32_t hardware_info) {
    writew86(hardware_info++, 16 << 10); hardware_info++;
    writew86(hardware_info++, 0); hardware_info++;
    writew86(hardware_info++, get_emm_pages_map_size()); hardware_info++;
    writew86(hardware_info++, 0); hardware_info++;
    writew86(hardware_info, 0);
}

uint16_t allocate_emm_pages_sys(uint16_t handler, uint16_t pages) {
    if (handler >= MAX_EMM_HANDLERS) {
        return 0x8500;
    }
    if (pages > TOTAL_EMM_PAGES) {
        return 0x8700; // There aren't enough expanded memory pages
    }
    emm_handler_t * h = &handlers[handler];
    if (allocated_emm_pages() - h->pages_acllocated + pages > TOTAL_EMM_PAGES) {
        return 0x8800; // There aren't enough unallocated pages
    }
    h->handler_in_use = true;
    h->pages_acllocated = pages;
    return 0;
}

uint16_t allocate_emm_raw_pages(uint16_t pages) {
    // TODO:
    return 0x86;
}

void custom_on_board_emm() {
    char tmp[90];
    if_reboot_detected();
    uint16_t FN = CPU_AH;
    // sprintf(tmp, "LIM40 FN %Xh", FN); logMsg(tmp);
    switch(CPU_AH) { // EMM LIM 4.0
    // The Get Status function returns a status code indicating whether the
    // memory manager is present and the hardware is working correctly.
    case 0x40: {
        CPU_AX = 0;
        logMsg("LIM40 FN 40h status: 0");
        zf = 0;
        return;
    }
    // The Get Page Frame Address function returns the segment address where
    // the page frame is located.
    case 0x41: {
        CPU_BX = emm_conventional_segment(); // page frame segment address
        sprintf(tmp, "LIM40 FN %Xh -> 0x%X (page frame segment)", FN, CPU_BX); logMsg(tmp);
        CPU_AX = 0;
        zf = 0;
        return;
    }
    // The Get Unallocated Page Count function returns the number of
    // unallocated pages and the total number of expanded memory pages.
    case 0x42: {
        CPU_BX = unallocated_emm_pages();
        CPU_DX = total_emm_pages();
        sprintf(tmp, "LIM40 FN %Xh -> %d free of %d EMM pages", FN, CPU_BX, CPU_DX); logMsg(tmp);
        CPU_AX = 0;
        zf = 0;
        return;
    }
    // The Allocate Pages function allocates the number of pages requested
    // and assigns a unique EMM handle to these pages. The EMM handle owns
    // these pages until the application deallocates them.
    case 0x43: {
        CPU_DX = allocate_emm_pages(CPU_BX, &CPU_AX);
        sprintf(tmp, "LIM40 FN %Xh err: %Xh alloc(%d pages); handler: %d", FN, CPU_AH, CPU_BX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Map/Unmap Handle Page function maps a logical page at a specific
    // physical page anywhere in the mappable regions of system memory.
    case 0x44: {
        // AL = physical_page_number
        // BX = logical_page_number, if FFFFh, the physical page specified in AL will be unmapped
        // DX = emm_handle
        uint8_t AL = CPU_AL;
        CPU_AX = map_unmap_emm_page(CPU_AL, CPU_BX, CPU_DX);
        sprintf(tmp, "LIM40 FN %Xh res: phys page %d was mapped to %d logical for %d EMM handler",
                      FN, AL, CPU_BX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // Deallocate Pages deallocates the logical pages currently allocated to an EMM handle.
    case 0x45: {
        uint16_t emm_handle = CPU_DX;
        CPU_AX = deallocate_emm_pages(emm_handle);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh - EMM handler %d dealloc", FN, CPU_AX, emm_handle); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Get Version function returns the version number of the memory manager software.
    case 0x46: {
        /*
                                     0100 0000
                                       /   \
                                      4  .  0
        */
        CPU_AL = 0b01000000; // 4.0
        logMsg("LIM40 FN 46h res: 4.0");
        CPU_AH = 0; zf = 0;
        return;
    }
    // Save Page Map saves the contents of the page mapping registers on all
    // expanded memory boards in an internal save area.
    case 0x47: {
        CPU_AX = save_emm_mapping(CPU_DX);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh (save mapping for %d)", FN, CPU_AX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Restore Page Map function restores the page mapping register
    // contents on the expanded memory boards for a particular EMM handle.
    // This function lets your program restore the contents of the mapping
    // registers its EMM handle saved.
    case 0x48: {
        CPU_AX = restore_emm_mapping(CPU_DX);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh (restore mapping for %d)", FN, CPU_AX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Get Handle Count function returns the number of open EMM handles
    // (including the operating system handle 0) in the system.
    case 0x4B: {
        CPU_BX = total_open_emm_handles();
        sprintf(tmp, "LIM40 FN %Xh res: %Xh - total_emm_handlers", FN, CPU_BX); logMsg(tmp);
        CPU_AX = 0; zf = 0;
        return;
    }
    // The Get Handle Pages function returns the number of pages allocated to
    // a specific EMM handle.
    case 0x4C: {
        CPU_BX = get_emm_handle_pages(CPU_DX, &CPU_AX);
        sprintf(tmp, "LIM40 FN %Xh handler: %d; res: %Xh; pages handled: %d", FN, CPU_DX, CPU_AX, CPU_BX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // The Get All Handle Pages function returns an array of the open emm
    // handles and the number of pages allocated to each one.
    case 0x4D: {
        // ES:DI = pointer to handle_page
        uint32_t addr32 = ((uint32_t)CPU_ES << 4) + CPU_DI;
        CPU_BX = get_all_emm_handle_pages(addr32);
        sprintf(tmp, "LIM40 FN %Xh all_handlers: %Xh (pages)", FN, CPU_BX); logMsg(tmp);
        CPU_AX = 0; zf = 0;
        return;
    }
    case 0x4E:
        FN = CPU_AX;
        switch(CPU_AL) {
        // The Get Page Map subfunction saves the mapping context for all
        // mappable memory regions (conventional and expanded) by copying the
        // contents of the mapping registers from each expanded memory board to a
        // destination array.
        case 0x00: {
            // ES:DI = dest_page_map
            uint32_t addr32 = ((uint32_t)CPU_ES << 4) + CPU_DI;
            get_emm_pages_map(addr32);
            sprintf(tmp, "LIM40 FN %Xh emm pages: %Xh done", FN, CPU_AX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // The Set Page Map subfunction restores the mapping context for all
        // mappable memory regions (conventional and expanded) by copying the
        // contents of a source array into the mapping registers on each expanded
        // memory board in the system.
        case 0x01: {
            // DS:SI = source_page_map
            uint32_t addr32 = ((uint32_t)CPU_DS << 4) + CPU_SI;
            set_emm_pages_map(addr32);
            sprintf(tmp, "LIM40 FN %Xh (save emm) done", FN); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // The Get & Set subfunction simultaneously saves a current mapping
        // context and restores a previous mapping context for all mappable
        // memory regions (both conventional and expanded).
        case 0x02: {
            // ES:DI = dest_page_map
            // DS:SI = source_page_map
            get_emm_pages_map(((uint32_t)CPU_ES << 4) + CPU_DI);
            set_emm_pages_map(((uint32_t)CPU_DS << 4) + CPU_SI);
            sprintf(tmp, "LIM40 FN %Xh (get/save) done", FN); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // The Get Size of Page Map Save Array subfunction returns the storage
        // requirements for the array passed by the other three subfunctions.
        // This subfunction doesn't require an EMM handle.
        case 0x03: {
            CPU_AX = get_emm_pages_map_size();
            sprintf(tmp, "LIM40 FN %Xh emm pages size: %Xh", FN, CPU_AX); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    case 0x4F:
        FN = CPU_AX;
        switch(CPU_AL) {
        // The Get Partial Page Map subfunction saves a partial mapping context
        // for specific mappable memory regions in a system.
        case 0x00: {
            // DS:SI = partial_page_map
            // ES:DI = dest_array
            CPU_AX = get_partial_emm_page_map((CPU_DS << 4) + CPU_SI, (CPU_ES << 4) + CPU_DI);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_partial_emm_page_map (w/a)", FN, CPU_AH); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        case 0x01: {
            // DS:SI = source_array
            CPU_AX = set_partial_emm_page_map((CPU_DS << 4) + CPU_SI);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh set_partial_emm_page_map (w/a)", FN, CPU_AH); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // The Return Size subfunction returns the storage requirements for the
        // array passed by the other two subfunctions.  This subfunction doesn't
        // require an EMM handle.
        case 0x03: {
            // BX = number of pages in the partial array
            CPU_AX = get_emm_pages_map_size(); // W/A
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_emm_pages_map_size (w/a)", FN, CPU_AX); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    case 0x50:
        FN = CPU_AX;
        switch(CPU_AL) {
        // MAPPING AND UNMAPPING MULTIPLE PAGES
        case 0x00: {
            uint16_t handle = CPU_DX;
            uint16_t log_to_phys_map_len = CPU_CX;
            uint32_t log_to_phys_map = ((uint32_t)CPU_DS << 4) + CPU_SI;
            CPU_AX = map_unmap_emm_pages(handle, log_to_phys_map_len, log_to_phys_map);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh map_unmap_emm_pages(%d, %d, %Xh)",
                    FN, CPU_AX, handle, log_to_phys_map_len, log_to_phys_map); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // MAP/UNMAP MULTIPLE HANDLE PAGES (by physical segments)
        case 0x01: {
            uint16_t handle = CPU_DX;
            uint16_t log_to_segment_map_len = CPU_CX;
            uint32_t log_to_segment_map = ((uint32_t)CPU_DS << 4) + CPU_SI;
            CPU_AX = map_unmap_emm_seg_pages(handle, log_to_segment_map_len, log_to_segment_map);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh map_unmap_emm_seg_pages(%d, %d, %Xh)",
                    FN, CPU_AX, handle, log_to_segment_map_len, log_to_segment_map); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    // REALLOCATE PAGES
    case 0x51: {
        CPU_AX = reallocate_emm_pages(CPU_DX, CPU_BX);
        sprintf(tmp, "LIM40 FN %Xh err: %Xh realloc(%d pages); handler: %d", FN, CPU_AH, CPU_BX, CPU_DX); logMsg(tmp);
        if (CPU_AX) zf = 1; else zf = 0;
        return;
    }
    // Optional: set handler attributes
    case 0x52: {
        sprintf(tmp, "LIM40 FN %Xh err: 91h Optional: set handler attributes (not implemented)", FN); logMsg(tmp);
        CPU_AX = 0x9100; // not supported
        zf = 1;
        return;
    }
    case 0x53:
        FN = CPU_AX;
        switch(CPU_AL) {
        // GET HANDLE NAME 
        case 0x00: {
            uint16_t handle = CPU_DX;
            uint32_t handle_name = ((uint32_t)CPU_ES << 4) + CPU_DI;
            CPU_AX = get_handle_name(handle, handle_name);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_handle_name(%d, %Xh)",
                    FN, CPU_AX, handle, handle_name); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // SET HANDLE NAME
        case 0x01: {
            uint16_t handle = CPU_DX;
            uint32_t handle_name = ((uint32_t)CPU_DS << 4) + CPU_SI;
            CPU_AX = set_handle_name(handle, handle_name);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_handle_name(%d, %Xh)",
                    FN, CPU_AX, handle, handle_name); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    case 0x54:
        FN = CPU_AX;
        switch(CPU_AL) {
        // GET HANDLE DIRECTORY
        case 0x00: {
            uint32_t handle_dir_struct = ((uint32_t)CPU_ES << 4) + CPU_DI;
            CPU_AX = get_handle_dir(handle_dir_struct);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh handle_dir_struct(%Xh)", FN, CPU_AX, handle_dir_struct); logMsg(tmp);
            if (CPU_AH) zf = 1; else zf = 0;
            return;
        }
        // SEARCH FOR NAMED HANDLE
        case 0x01: {
            uint32_t handle_name = ((uint32_t)CPU_ES << 4) + CPU_DI;
            CPU_DX = lookup_handle_dir(handle_name , &CPU_AX);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh handle: %d lookup_handle_dir(%Xh)", FN, CPU_AH, CPU_DX, handle_name); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // GET TOTAL HANDLES
        case 0x02: {
            CPU_BX = MAX_EMM_HANDLERS;
            sprintf(tmp, "LIM40 FN %Xh MAX_EMM_HANDLERS: %d", FN, CPU_BX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // TODO: 
    }
    // ALTER PAGE MAP & JUMP
    case 0x55: {
        uint8_t page_number_segment_selector = CPU_AL;
        uint16_t handle = CPU_DX;
        uint32_t map_and_jump = ((uint32_t)CPU_DS << 4) + CPU_SI;
        CPU_AH = map_emm_and_jump(page_number_segment_selector, handle, map_and_jump);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh handle: %d page_number_segment_selector: %d (not implemented)",
                      FN, CPU_AH, CPU_DX, page_number_segment_selector); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    // ALTER PAGE MAP & CALL
    case 0x56: {
        uint8_t page_number_segment_selector = CPU_AL;
        uint16_t handle = CPU_DX;
        uint32_t map_and_call = ((uint32_t)CPU_DS << 4) + CPU_SI;
        CPU_AH = map_emm_and_call(page_number_segment_selector, handle, map_and_call);
        sprintf(tmp, "LIM40 FN %Xh res: %Xh handle: %d page_number_segment_selector: %d (not implemented)",
                      FN, CPU_AH, CPU_DX, page_number_segment_selector); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    // MOVE/EXCHANGE MEMORY REGION
    case 0x57: {
        CPU_AH = 0x86;
        sprintf(tmp, "LIM40 FN %Xh MOVE/EXCHANGE MEMORY REGION (not implemented)", FN); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    case 0x58:
        FN = CPU_AX;
        switch(CPU_AL) {
        // GET MAPPABLE PHYSICAL ADDRESS ARRAY
        case 0x00: {
            uint32_t mappable_phys_page = ((uint32_t)CPU_ES << 4) + CPU_DI;
            CPU_AX = get_mappable_physical_array(mappable_phys_page);
            sprintf(tmp, "LIM40 FN %Xh res: %Xh get_mappable_physical_array(%Xh)",
                    FN, CPU_AX, mappable_phys_page); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        // GET MAPPABLE PHYSICAL ADDRESS ARRAY ENTRIES
        case 0x01: {
            CPU_CX = get_mappable_phys_pages();
            sprintf(tmp, "LIM40 FN %Xh get_mappable_phys_pages: %d", FN, CPU_CX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
    }
    case 0x59:
        FN = CPU_AX;
        switch(CPU_AL) {
        // GET HARDWARE CONFIGURATION ARRAY
        case 0x00: {
            uint32_t hardware_info = ((uint32_t)CPU_ES << 4) + CPU_DI;
            get_hardvare_emm_info(hardware_info);
            sprintf(tmp, "LIM40 FN %Xh GET HARDWARE CONFIGURATION ARRAY", FN); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
        // GET UNALLOCATED RAW PAGE COUNT
        case 0x01: {
            CPU_BX = unallocated_emm_pages();
            CPU_DX = total_emm_pages();
            sprintf(tmp, "LIM40 FN %Xh %d of %d free pages", FN, CPU_BX, CPU_DX); logMsg(tmp);
            CPU_AX = 0; zf = 0;
            return;
        }
    }
    case 0x5A:
        FN = CPU_AX;
        switch(CPU_AL) {
        //  ALLOCATE STANDARD PAGES
        case 0x00: {
            CPU_AX = allocate_emm_pages_sys(CPU_BX, CPU_DX);
            sprintf(tmp, "LIM40 FN %Xh (%d, %d) allocate_emm_pages_sys res: %Xh", FN, CPU_BX, CPU_DX, CPU_AX); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
        //  ALLOCATE STANDARD/RAW
        case 0x01: {
            CPU_AX = allocate_emm_raw_pages(CPU_BX);
            sprintf(tmp, "LIM40 FN %Xh (%d, %d) allocate_emm_raw_pages res: %Xh (not yet)", FN, CPU_BX, CPU_DX, CPU_AX); logMsg(tmp);
            if (CPU_AX) zf = 1; else zf = 0;
            return;
        }
    }
    case 0x5B: {
        CPU_AH = 0x86;
        sprintf(tmp, "LIM40 FN %Xh ALTERNATE MAP REGISTER SET (not implemented)", FN); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    case 0x5C: {
        CPU_AH = 0x00;
        sprintf(tmp, "LIM40 FN %Xh PREPARE EXPANDED MEMORY HARDWARE FOR WARM BOOT", FN); logMsg(tmp);
        extra_mem_initialized = false;
        zf = 0;
        return;
    }
    case 0x5D: {
        CPU_AH = 0x86;
        sprintf(tmp, "LIM40 FN %Xh ENABLE/DISABLE OS/E FUNCTION SET (not implemented)", FN); logMsg(tmp);
        if (CPU_AH) zf = 1; else zf = 0;
        return;
    }
    default:
        sprintf(tmp, "LIM40 FN %Xh (not implemented)", CPU_AX); logMsg(tmp);
    }
}

void emm_reboot() {
   memset(emm_saved_tables, 0, sizeof emm_saved_tables);
   memset(emm_desc_table, 0, sizeof emm_desc_table);
   memset(handlers, 0, sizeof handlers);
   init_emm();
}
