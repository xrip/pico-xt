#pragma once
#include <stdbool.h>
#include <inttypes.h>

#ifdef XMS_UMB
  #define UMB_START_ADDRESS 0xC0000ul
  #define UMB_BLOCKS 6
  #ifdef XMS_HMA
    #define RESERVED_XMS_KB (5 * 32 + 24 /*0x6000*/ + 64 /*HMA*/)
  #else
    #define RESERVED_XMS_KB (UMB_BLOCKS * 32)
  #endif
#else
  #define RESERVED_XMS_KB 0
#endif

#define XMS_ERROR_CODE   0x0000
#define XMS_SUCCESS_CODE 0x0001

#ifdef XMS_UMB
void init_umb();
uint16_t umb_allocate(uint16_t* psz, uint16_t* err);
uint16_t umb_deallocate(uint16_t* seg, uint16_t* err);
uint8_t* pBIOS();
uint8_t* pBASICL();
uint8_t* pBASICH();
#endif
